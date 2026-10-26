from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]

class BenchmarkContract(unittest.TestCase):
    def test_opt_in_and_quiet(self):
        main = (ROOT/'src/SDL/main.c').read_text()
        diagnostics = (ROOT/'src/Modern/WindowsDiagnostics.cpp').read_text()
        self.assertIn('entryVr("/benchmark"', main)
        self.assertIn('L"-benchmark", L"/benchmark"', diagnostics)
        self.assertIn('startupBenchmark && (pEvent->type == SDL_KEYDOWN', main)

    def test_queries_reuse_existing_fence_and_match_capture(self):
        source = (ROOT/'src/Modern/ModernGraphics.cpp').read_text()
        self.assertIn('frame.benchmarkCaptureId == benchmark.id', source)
        self.assertIn('frame.benchmarkCaptureId = benchmark.recording ? benchmark.id : 0', source)
        self.assertIn('(!benchmark.enabled || optimizationLogging) && (presenter.frameCount % telemetryInterval)', source)
        self.assertIn('if (benchmark.enabled) return;', source)
        capture = (ROOT/'src/Modern/BenchmarkCapture.inc').read_text()
        self.assertNotIn('WaitForSingleObject', capture)
        self.assertNotIn('Flush', capture)
        self.assertIn('benchmarkFinish(true)', capture)
        self.assertIn('>= 30.0', capture)
        self.assertIn('>= 5.0', capture)

    def test_simulation_scope_preserves_return(self):
        source = (ROOT/'src/Game/UnivUpdate.c').read_text()
        self.assertIn('result = univUpdateImpl(phystimeelapsed);', source)
        self.assertIn('hwModernBenchmarkCpuEnd(1);\n    return result;', source)

if __name__ == '__main__':
    unittest.main()
