declare global {
  interface Window {
    ipc: { postMessage(payload: string): void };
    openhwReply(id: number, ok: boolean, value: unknown): void;
  }
}
let nextId = 0;
const pending = new Map<number, {resolve(value: unknown): void; reject(value: unknown): void}>();
window.openhwReply = (id, ok, value) => {
  const call = pending.get(id);
  if (!call) return;
  pending.delete(id);
  if (ok) call.resolve(value); else call.reject(value);
};
export function invoke<T = unknown>(command: string, args: object = {}): Promise<T> {
  return new Promise<T>((resolve, reject) => {
    const id = ++nextId;
    pending.set(id, {resolve: value => resolve(value as T), reject});
    try {window.ipc.postMessage(JSON.stringify({id, command, args}));}
    catch (e) {pending.delete(id); reject(e);}
  });
}
