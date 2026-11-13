"""Local HTTP and hostile ZIP regression tests; no live website or game required."""
from dataclasses import replace
import hashlib
import http.client
import importlib.util
import io
import json
from pathlib import Path
import socket
import stat
import struct
import tempfile
import threading
import unittest
from unittest import mock
import warnings
import zipfile

from configure_diagnostics import add_route
from diagnostics import ENDPOINT, IntakeServer, Limits, ReportError, validate_archive


def report_zip(files=None, metadata=None, extras=(), compression=zipfile.ZIP_DEFLATED):
    files = {'logs/renderer.log': b'Example log\n'} if files is None else files
    data = {'schema': 1, 'alias': 'Community Pilot', 'comment': 'Camera stutter',
            'created_utc': '2026-11-13T12:00:00Z', 'game_version': '1.1',
            'files': [{'path': name, 'size': len(body)} for name, body in files.items()]}
    if metadata:
        data.update(metadata)
    output = io.BytesIO()
    with warnings.catch_warnings():
        warnings.simplefilter('ignore', UserWarning)
        with zipfile.ZipFile(output, 'w', compression=compression) as archive:
            archive.writestr('report.json', json.dumps(data))
            for name, body in files.items():
                archive.writestr(name, body)
            for name, body in extras:
                archive.writestr(name, body)
    return output.getvalue()


class ArchiveTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.path = Path(self.temporary.name) / 'report.zip'
        self.limits = Limits(free_bytes=0)

    def validate(self, data, limits=None):
        self.path.write_bytes(data)
        return validate_archive(self.path, limits or self.limits)

    def reject(self, data, status=400, limits=None):
        with self.assertRaises(ReportError) as caught:
            self.validate(data, limits)
        self.assertEqual(caught.exception.status, status)

    def test_valid_metadata_and_extensionless_save(self):
        for suffix in ('', '.sav', '.save', '.game'):
            with self.subTest(suffix=suffix):
                result = self.validate(report_zip({'logs/debug.dmp': b'\0\1', 'saves/01 - Kharak System' + suffix: b'SAVE'}))
                self.assertEqual(result['game_version'], '1.1')
                self.assertEqual(result['files'][1]['size'], 4)

    def test_high_ratio_repetitive_log_is_accepted(self):
        content = b'A' * (2 * 1024 * 1024)
        data = report_zip({'logs/repeated.log': content})
        self.assertLess(len(data), len(content) // 100)
        self.assertEqual(self.validate(data)['files'][0]['size'], len(content))

    def test_bad_paths_and_file_types(self):
        for name in ('../outside.log', '/logs/a.log', 'logs/../outside.log', 'logs/a/b.log',
                     'logs\\a.log', 'logs/a.log ', 'logs/a.exe', 'saves/a.exe', 'logs/C:bad.log'):
            with self.subTest(name=name):
                self.reject(report_zip({name: b'x'}))

    def test_symlink_duplicate_case_and_unicode(self):
        link = zipfile.ZipInfo('logs/link.log')
        link.create_system = 3
        link.external_attr = (stat.S_IFLNK | 0o777) << 16
        self.reject(report_zip(extras=[(link, b'../../elsewhere')]))
        self.reject(report_zip(extras=[('logs/renderer.log', b'x')]))
        self.reject(report_zip(extras=[('logs/RENDERER.LOG', b'x')]))
        self.reject(report_zip({'logs/caf\u00e9.log': b'a', 'logs/cafe\u0301.log': b'b'}))

    def test_crc_damage_is_rejected(self):
        data = bytearray(report_zip(compression=zipfile.ZIP_STORED))
        with zipfile.ZipFile(io.BytesIO(data)) as archive:
            member = archive.getinfo('logs/renderer.log')
            offset = member.header_offset
            name_length, extra_length = struct.unpack_from('<HH', data, offset + 26)
            data[offset + 30 + name_length + extra_length] ^= 1
        self.reject(data)

    def test_inventory_and_metadata_are_validated(self):
        for metadata in ({'schema': True}, {'schema': 2}, {'alias': ''}, {'alias': 'x' * 81},
                         {'alias': 'bad\nname'}, {'comment': 'x' * 4001}, {'game_version': 3},
                         {'game_version': 'x' * 33}, {'created_utc': '2026-01-01'},
                         {'created_utc': '2026-01-01T00:00:00-04:00'}, {'unexpected': 'field'},
                         {'files': [{'path': 'logs/renderer.log', 'size': 1}]},
                         {'files': [{'path': 'logs/renderer.log', 'size': True}]}):
            with self.subTest(metadata=metadata):
                self.reject(report_zip(metadata=metadata))
        self.reject(report_zip(extras=[('saves/a', b'a'), ('saves/b', b'b')]))

    def test_size_member_and_directory_preflight_limits(self):
        self.reject(report_zip({'logs/large.txt': b'X' * 10000}), 413, replace(self.limits, expanded_bytes=1000))
        self.reject(report_zip({'logs/a.txt': b'a', 'logs/b.txt': b'b'}), limits=replace(self.limits, members=2))
        data = bytearray(report_zip())
        offset = data.rfind(b'PK\x05\x06')
        struct.pack_into('<HH', data, offset + 8, 65535, 65535)
        self.reject(data)
        self.reject(b'not a zip')
        self.reject(report_zip() + b'trailing content')


class HttpTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name) / 'reports'
        self.server = IntakeServer(('127.0.0.1', 0), self.root, Limits(free_bytes=0, per_ip_requests=100))
        self.thread = threading.Thread(target=self.server.serve_forever, kwargs={'poll_interval': .01}, daemon=True)
        self.thread.start()
        self.addCleanup(self.stop)

    def stop(self):
        self.server.shutdown()
        self.thread.join(timeout=2)
        self.server.server_close()

    def request(self, body=None, method='POST', path=ENDPOINT, headers=None):
        connection = http.client.HTTPConnection(*self.server.server_address, timeout=3)
        try:
            connection.request(method, path, body, {'Content-Type': 'application/zip', **(headers or {})})
            response = connection.getresponse()
            data = response.read()
            return response.status, json.loads(data) if data else None
        finally:
            connection.close()

    def raw(self, headers, body=b''):
        connection = http.client.HTTPConnection(*self.server.server_address, timeout=3)
        try:
            connection.putrequest('POST', ENDPOINT)
            for name, value in headers:
                connection.putheader(name, value)
            connection.endheaders(body)
            response = connection.getresponse()
            return response.status, json.loads(response.read())
        finally:
            connection.close()

    def test_success_private_archive_no_extraction(self):
        archive = report_zip(metadata={'alias': '../../Pilot ☃'})
        status, reply = self.request(archive)
        self.assertEqual(status, 201)
        report_id = reply['report_id']
        self.assertRegex(report_id, r'^[A-Za-z0-9_-]{1,160}$')
        directory = self.root / report_id
        self.assertEqual({p.name for p in directory.iterdir()}, {'report.json', 'report.zip'})
        self.assertEqual((directory / 'report.zip').read_bytes(), archive)
        metadata = json.loads((directory / 'report.json').read_text('utf-8'))
        self.assertEqual(metadata['archive_sha256'], hashlib.sha256(archive).hexdigest())
        self.assertEqual(metadata['alias'], '../../Pilot ☃')
        self.assertEqual(self.request(method='GET', path=ENDPOINT + '/' + report_id)[0], 404)
        self.assertEqual(self.request(method='HEAD')[0], 404)

    def test_real_client_zip64_stream_and_upload_contract(self):
        path = Path(__file__).resolve().parents[1] / 'tools/openhw_diagnostics.py'
        spec = importlib.util.spec_from_file_location('openhw_report_client_test', path)
        client = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(client)
        installation = Path(self.temporary.name) / 'installation'
        (installation / 'Development/Logs').mkdir(parents=True)
        (installation / 'SavedGames/SinglePlayer').mkdir(parents=True)
        (installation / 'Development/Logs/renderer.log').write_bytes(b'Client-generated report\n' * 1000)
        (installation / 'SavedGames/SinglePlayer/01 - Kharak System').write_bytes(b'SAVE')
        (installation / 'version.json').write_text('{"version":"1.1"}', encoding='utf-8')
        archive = client.build_report(installation, 'Client Pilot', 'Contract test',
            ['Development/Logs/renderer.log'], 'SavedGames/SinglePlayer/01 - Kharak System')
        host, port = self.server.server_address
        with mock.patch.object(client, 'ENDPOINT', f'http://{host}:{port}{ENDPOINT}'):
            report_id = client.upload_report(archive)
        saved = json.loads((self.root / report_id / 'report.json').read_text('utf-8'))
        self.assertEqual(saved['game_version'], '1.1')
        self.assertEqual(saved['files'][1]['path'], 'saves/01 - Kharak System')
        self.assertTrue(archive.with_suffix('.receipt.json').is_file())

    def test_http_headers_path_and_compressed_cap(self):
        archive = report_zip()
        self.assertEqual(self.request(archive, headers={'Content-Type': 'text/plain'})[0], 415)
        self.assertEqual(self.request(archive, path=ENDPOINT + '/extra')[0], 404)
        self.server.limits = replace(self.server.limits, body_bytes=len(archive) - 1)
        self.assertEqual(self.request(archive)[0], 413)
        self.assertEqual(self.raw([('Content-Type', 'application/zip')])[0], 411)
        self.assertEqual(self.raw([('Content-Type', 'application/zip'), ('Content-Length', '1'), ('Content-Length', '1')], b'x')[0], 411)
        self.assertEqual(self.raw([('Content-Type', 'application/zip'), ('Transfer-Encoding', 'chunked')], b'0\r\n\r\n')[0], 400)

    def test_busy_quota_rate_and_no_public_retrieval(self):
        archive = report_zip()
        self.server._uploads.acquire()
        self.server._uploads.acquire()
        try:
            self.assertEqual(self.request(archive)[0], 503)
        finally:
            self.server._uploads.release()
            self.server._uploads.release()
        self.server.limits = replace(self.server.limits, storage_bytes=1)
        self.assertEqual(self.request(archive)[0], 507)
        self.assertEqual(list(self.root.iterdir()), [])
        self.server.limits = replace(self.server.limits, per_ip_requests=1)
        self.assertEqual(self.request(archive)[0], 429)

    def test_forwarded_address_single_value_and_bad_archive_cleanup(self):
        archive = report_zip()
        self.server.limits = replace(self.server.limits, per_ip_requests=1)
        self.assertEqual(self.request(b'not a zip', headers={'X-Forwarded-For': '192.0.2.1'})[0], 400)
        self.assertEqual(self.request(archive, headers={'X-Forwarded-For': '192.0.2.1'})[0], 429)
        self.assertEqual(self.request(archive, headers={'X-Forwarded-For': '192.0.2.2'})[0], 201)
        self.assertEqual(self.request(b'x', headers={'X-Forwarded-For': '192.0.2.3, 192.0.2.4'})[0], 400)
        self.assertEqual(self.request(archive, headers={'X-Forwarded-For': 'invalid'})[0], 429)
        self.assertFalse(list(self.root.glob('.upload-*')))

    def test_stalled_body_times_out(self):
        self.server.limits = replace(self.server.limits, socket_seconds=.1, upload_seconds=.2)
        status, _ = self.raw([('Content-Type', 'application/zip'), ('Content-Length', '100')], b'x')
        self.assertEqual(status, 408)

    def test_incomplete_headers_cannot_hold_a_connection(self):
        self.server.limits = replace(self.server.limits, socket_seconds=.1)
        with socket.create_connection(self.server.server_address, timeout=2) as connection:
            connection.sendall(b'POST /diagnostics/v1/reports HTTP/1.1\r\nHost: localhost\r\n')
            self.assertEqual(connection.recv(1024), b'')

    def test_refuses_public_bind(self):
        with self.assertRaises(ValueError):
            IntakeServer(('0.0.0.0', 0), self.root)


    def profile_client(self):
        path = Path(__file__).resolve().parents[1] / 'tools/openhw_diagnostics.py'
        spec = importlib.util.spec_from_file_location('openhw_profile_client_test', path)
        client = importlib.util.module_from_spec(spec); spec.loader.exec_module(client)
        host, port = self.server.server_address
        client.ENDPOINT = f'http://{host}:{port}{ENDPOINT}'
        installation = Path(self.temporary.name) / 'installation'
        (installation / 'Development/Logs').mkdir(parents=True, exist_ok=True)
        return client, installation

    def test_named_folder_is_created_on_save_and_reused(self):
        client, root = self.profile_client(); state = client.load_state(root)
        profile = client.save_profile(root, state, '../../Community Pilot', 'First comment')
        folder = self.root / profile['profile_id']
        self.assertTrue((folder / 'profile.json').is_file())
        record = json.loads((folder / 'profile.json').read_text('utf-8'))
        self.assertNotIn(profile['upload_token'], (folder / 'profile.json').read_text())
        self.assertEqual(record['name'], '../../Community Pilot')
        updated = client.save_profile(root, state, 'Renamed Pilot', 'New comment', profile)
        self.assertEqual(updated['profile_id'], profile['profile_id'])
        self.assertEqual(len(list(self.root.iterdir())), 1)
        self.assertEqual(client.load_state(root)['profiles'][0]['comment'], 'New comment')
        self.assertEqual(self.request(method='GET', path='/diagnostics/v1/profiles')[0], 404)
        self.assertEqual(self.request(method='GET', path='/diagnostics/v1/profiles/'+profile['profile_id'])[0], 404)

    def test_all_logs_batching_dedup_and_changed_contents(self):
        client, root = self.profile_client(); state = client.load_state(root)
        profile = client.save_profile(root, state, 'Pilot', 'All new logs')
        for i in range(205): (root / f'Development/Logs/{i:03}.log').write_bytes(b'log')
        (root / 'Development/Logs/nested').mkdir()
        (root / 'Development/Logs/nested/000.log').write_bytes(b'different same basename')
        (root / 'private.txt').write_text('not sent')
        self.assertEqual(client.upload_pending(root, state, profile), 206)
        folder = self.root / profile['profile_id']
        self.assertEqual(len(list(folder.glob('*.zip'))), 4)
        self.assertEqual(client.pending_logs(root, client.load_state(root)), [])
        self.assertEqual(client.upload_pending(root, state, profile), 0)
        path = root / 'Development/Logs/000.log'
        # Same length, same name and mtime still needs upload if contents changed.
        timestamp = path.stat().st_mtime; path.write_bytes(b'new')
        import os
        os.utime(path, (timestamp, timestamp))
        self.assertEqual(client.upload_pending(root, state, profile), 1)
        self.assertEqual(len(list(folder.glob('*.zip'))), 5)
        for archive_path in folder.glob('*.zip'):
            with zipfile.ZipFile(archive_path) as archive:
                meta = json.loads(archive.read('report.json'))
                self.assertTrue(all(x['source'].startswith('Development/Logs/') for x in meta['files']))
                self.assertFalse(any('private' in name for name in archive.namelist()))

    def test_large_log_chunks_and_appended_live_log(self):
        client, root = self.profile_client(); client.CHUNK_BYTES = 4
        state = client.load_state(root); profile = client.save_profile(root, state, 'Pilot', '')
        log = root / 'Development/Logs/renderer.log'; log.write_bytes(b'abcdefghij')
        self.assertEqual(len(client.pending_logs(root, state)), 3)
        self.assertEqual(client.upload_pending(root, state, profile), 1)
        log.write_bytes(b'abcdefghijklm')
        chunks = client.pending_logs(root, state)
        self.assertEqual([item['offset'] for item in chunks], [8,12])
        self.assertEqual(client.upload_pending(root, state, profile), 1)
        self.assertEqual(client.pending_logs(root, state), [])

    def test_failed_batch_is_not_marked_uploaded_and_resume_keeps_success(self):
        client, root = self.profile_client(); state = client.load_state(root)
        profile = client.save_profile(root, state, 'Pilot', '')
        for i in range(61): (root / f'Development/Logs/{i}.log').write_bytes(b'data')
        original = client.upload_report
        calls = []
        def upload(path, profile):
            calls.append(path)
            if len(calls) == 2: raise OSError('connection interrupted')
            return original(path, profile)
        with mock.patch.object(client, 'upload_report', side_effect=upload):
            with self.assertRaises(OSError): client.upload_pending(root, state, profile)
        reloaded = client.load_state(root)
        self.assertEqual(len(reloaded['uploaded']), 60)
        self.assertEqual(client.upload_pending(root, reloaded, profile), 1)
        self.assertEqual(client.pending_logs(root, reloaded), [])

    def test_profile_token_prevents_folder_impersonation(self):
        client, root = self.profile_client(); state = client.load_state(root)
        profile = client.save_profile(root, state, 'Pilot', '')
        bad = dict(profile, upload_token='X'*43)
        from urllib.error import HTTPError
        with self.assertRaises(HTTPError) as error:
            client.save_profile(root, state, 'Impersonator', '', bad)
        self.assertEqual(error.exception.code, 403)
        (root / 'Development/Logs/renderer.log').write_bytes(b'data')
        with self.assertRaises(HTTPError) as error: client.upload_pending(root, state, bad)
        self.assertEqual(error.exception.code, 403)
        self.assertFalse(state['uploaded'])
        folder = self.root / profile['profile_id']
        self.assertEqual([p.name for p in folder.iterdir()], ['profile.json'])

    def test_changed_log_after_scan_does_not_mark_or_upload(self):
        client, root = self.profile_client(); state = client.load_state(root)
        profile = client.save_profile(root, state, 'Pilot', '')
        log = root / 'Development/Logs/renderer.log'; log.write_bytes(b'initial')
        chunks = client.pending_logs(root, state); log.write_bytes(b'changed')
        with self.assertRaises(ValueError): client.build_batch(root, profile, chunks)
        self.assertFalse(list((root / 'Development/Reports').glob('report-*.zip')))
        self.assertFalse(state['uploaded'])


class ConfigurationTests(unittest.TestCase):
    def test_route_is_idempotent_preserves_other_handlers(self):
        existing = ('openhw.tech {\n    handle /multiplayer/v1 {\n        reverse_proxy 127.0.0.1:8767\n    }\n'
                    '    handle /downloads/* {\n        root * /srv/openhw-downloads\n    }\n'
                    '    handle /api/* {\n        reverse_proxy 127.0.0.1:3000\n    }\n'
                    '    handle {\n        root * /srv/openhw-site\n        file_server\n    }\n}\n')
        updated = add_route(existing)
        self.assertEqual(add_route(updated), updated)
        self.assertEqual(updated.count('handle /diagnostics/v1/*'), 1)
        self.assertIn('header_up X-Forwarded-For {remote_host}', updated)
        self.assertTrue(updated.endswith(existing[existing.index('    handle /multiplayer/v1 {'):]))

    def test_refuses_ambiguous_or_unmanaged_route(self):
        for existing in ('site {\n}\n', '    handle /multiplayer/v1 {\n' * 2,
                         '    handle /multiplayer/v1 {\n    handle /diagnostics/v1/reports {\n'):
            with self.subTest(existing=existing), self.assertRaises(ValueError):
                add_route(existing)


if __name__ == '__main__':
    unittest.main()
