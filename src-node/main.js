import initWasm, { NodeGraphChain } from '../pkg/flexar_core.js';
import { FlexarRenderer } from './engine/FlexarRenderer.js';
import { NodeGraphOverlay } from './ui/NodeGraphOverlay.js';

async function bootstrap() {
  const wasm = await initWasm();
  const chain = new NodeGraphChain(20000);

  const canvas = document.getElementById('flexar-canvas');
  const renderer = new FlexarRenderer(canvas);

  // Initialize Interactive Node Overlay with parameter & graph connection callbacks
  const overlay = new NodeGraphOverlay(
    'app',
    // 1. Parameter Changed Callback
    (nodeId, key, value) => {
      if (key === 'speed') chain.set_speed(value);
      if (key === 'spread') chain.set_spread(value);
      if (key === 'intensity') renderer.bloomPass.strength = value * 3;
    },
    // 2. Graph Connections Changed Callback
    (connections) => {
      const isGlitchConnected = connections.some(c => c.from.startsWith('glitch') && c.to === 'output');
      renderer.glitchPass.enabled = isGlitchConnected;
    }
  );

  let startTime = performance.now();

  function animate(now) {
    const elapsedTime = (now - startTime) / 1000;

    chain.update(0.016, elapsedTime);

    const ptr = chain.get_particle_buffer_ptr();
    const size = chain.get_particle_buffer_size();
    const particleData = new Float32Array(wasm.memory.buffer, ptr, size);

    renderer.updateParticles(particleData);
    renderer.render();
    overlay.draw();

    requestAnimationFrame(animate);
  }

  requestAnimationFrame(animate);
}

bootstrap();
