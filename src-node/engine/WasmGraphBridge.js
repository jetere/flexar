// JavaScript wrapper calling Rust WASM graph
import initWasm, { NodeGraphChain } from '../../pkg/flexar_core.js';

export class WasmGraphBridge {
  async initialize(maxParticles = 50000) {
    this.wasm = await initWasm();
    // Allocate high-performance particle chain in Rust memory space
    this.chain = new NodeGraphChain(maxParticles);
  }

  updateAndFetchBuffer(deltaTime, elapsedTime) {
    // 1. Run full Rust effect node chain
    this.chain.update(deltaTime, elapsedTime);

    // 2. Fetch direct Float32Array zero-copy slice from Wasm memory heap
    const ptr = this.chain.get_particle_buffer_ptr();
    const size = this.chain.get_particle_buffer_size();

    return new Float32Array(this.wasm.memory.buffer, ptr, size);
  }
}
