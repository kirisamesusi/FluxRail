/** CDC v1 的唯一网页收发入口。只使用公共协议，不访问 HID、Flash 或实时运动。
 * 一次仅发一个请求；任何传输超时都关闭会话，防止迟到 ACK 被下一操作误用。
 * 超时不等于设备没执行：上层必须重新握手读回，不能自动重试写命令。
 */
export class ProtocolError extends Error {
  constructor(code, message) { super(message); this.code = code; }
}
const fail = (code, message) => new ProtocolError(code, message);
export class LineFramer {
  constructor(onLine, onError, limit = 4096) {
    this.onLine = onLine; this.onError = onError; this.limit = limit;
    this.line = ''; this.dropping = false;
  }
  feed(bytes) {
    // 逐字节限制而非拼接无限字符串；ASCII协议不用跨块UTF-8猜解。
    for (const byte of bytes) {
      if (byte === 10) {
        if (this.dropping) this.onError(fail('LINE_TOO_LONG', '设备响应超长或包含非ASCII字节'));
        else if (this.line.trim()) this.onLine(this.line);
        this.line = ''; this.dropping = false;
      } else if (!this.dropping) {
        if (byte > 127 || this.line.length >= this.limit) {
          this.dropping = true; this.line = '';
        } else this.line += String.fromCharCode(byte);
      }
    }
  }
}
export class SerialClient {
  constructor(port, onFault = () => {}) {
    this.port = port; this.onFault = onFault; this.id = 0;
    this.tail = Promise.resolve(); this.queued = 0; this.alive = false;
    this.pending = null; this.revision = null; this.last = null;
  }
  async open() {
    try {
      await this.port.open({ baudRate: 115200, bufferSize: 8192 });
      await this.port.setSignals({ dataTerminalReady: true });
      this.reader = this.port.readable.getReader();
      this.writer = this.port.writable.getWriter(); this.alive = true;
      this.framer = new LineFramer(line => this.receive(line), error => this.abort(error));
      this.readTask = this.readLoop();
    } catch (error) { await this.close(); throw error; }
  }
  async readLoop() {
    try {
      while (this.alive) {
        const { value, done } = await this.reader.read();
        if (done) { if (this.alive) this.abort(fail('DISCONNECTED', '串口已断开，请重新连接并读回')); break; }
        if (value) this.framer.feed(value);
      }
    } catch (error) { if (this.alive) this.abort(error); }
    finally { this.reader?.releaseLock(); this.reader = null; }
  }
  receive(line) {
    if (!this.alive) return;
    let response;
    try { response = JSON.parse(line); }
    catch { this.abort(fail('INVALID_JSON', '设备返回无效JSON，请重新连接')); return; }
    const pending = this.pending;
    if (!pending || response.protocol_version !== 1 || response.id !== pending.id ||
        typeof response.ok !== 'boolean') {
      this.abort(fail('UNEXPECTED_RESPONSE', '协议版本或请求ID不匹配，请重新连接并读回')); return;
    }
    if (response.revision !== undefined && (!Number.isInteger(response.revision) ||
        response.revision < 1 || response.revision > 0xffffffff)) {
      this.abort(fail('INVALID_RESPONSE', '设备revision无效')); return;
    }
    if (response.ok && (!response.result || typeof response.result !== 'object')) {
      this.abort(fail('INVALID_RESPONSE', '设备缺少result')); return;
    }
    const uint = value => Number.isInteger(value) && value >= 1 && value <= 0xffffffff;
    if (response.ok && (!uint(response.revision) || typeof response.dirty !== 'boolean' ||
        !(response.applied_revision === null || response.applied_revision === response.revision) ||
        !(response.saved_revision === null || uint(response.saved_revision)))) {
      this.abort(fail('INVALID_RESPONSE', '配置/保存版本元数据不完整，停止写入')); return;
    }
    this.revision = response.revision ?? this.revision; this.last = response;
    pending.resolve(response);
  }
  request(op, payload = {}, timeout = 2000) {
    if (!this.alive) return Promise.reject(fail('DISCONNECTED', '请先连接设备'));
    if (this.queued >= 8) return Promise.reject(fail('QUEUE_FULL', '请求队列已满，请等待当前操作'));
    this.queued++;
    const task = this.tail.then(() => this.send(op, payload, timeout));
    this.tail = task.catch(() => {});
    return task.finally(() => this.queued--);
  }
  async send(op, payload, timeout) {
    if (!this.alive) throw fail('DISCONNECTED', '会话已结束，未发送排队请求');
    this.id = this.id % 0xffffffff + 1;
    const id = this.id;
    const line = JSON.stringify({ protocol_version: 1, id, op, payload });
    if (/[^\x00-\x7f]/.test(line) || line.length > 4096) throw fail('INVALID_REQUEST', '请求必须为ASCII且不超过4096字节');
    let timer;
    const response = new Promise((resolve, reject) => {
      this.pending = { id, resolve, reject };
    });
    const deadline = new Promise((_, reject) => {
      timer = setTimeout(() => {
        const error = fail('TIMEOUT', '结果未知：设备可能已执行。重新连接读回后再决定下一步');
        this.abort(error); reject(error);
      }, timeout);
    });
    try {
      // 同时等待write和ACK，超时也覆盖写端背压；写失败终止会话。
      const writing = this.writer.write(new TextEncoder().encode(line + '\n'));
      writing.catch(error => this.abort(error));
      const [reply] = await Promise.race([Promise.all([response, writing]), deadline]);
      if (!reply.ok) throw fail(reply.error?.code || 'DEVICE_ERROR', reply.error?.message || '设备拒绝请求');
      return reply.result;
    } finally { clearTimeout(timer); this.pending = null; }
  }
  abort(error) {
    if (!this.alive) return;
    this.alive = false; this.pending?.reject(error); this.onFault(error);
    void this.close();
  }
  async close() {
    if (this.closing) return this.closing;
    this.alive = false;
    this.pending?.reject(fail('DISCONNECTED', '连接结束；已接受的设备操作需重连读回'));
    this.closing = (async () => {
      try { await this.reader?.cancel(); } catch { /* 拔线时cancel可失败。 */ }
      try { await this.readTask; } catch { /* readLoop已报告。 */ }
      try { await this.writer?.abort(); } catch { /* 同上。 */ }
      try { this.writer?.releaseLock(); } catch { /* 未取得锁时无操作。 */ }
      this.writer = null;
      try { await this.port.setSignals({ dataTerminalReady: false }); } catch { /* 拔线后忽略。 */ }
      try { await this.port.close(); } catch { /* 关闭仍尝试，保留原始错误。 */ }
    })();
    return this.closing;
  }
}
