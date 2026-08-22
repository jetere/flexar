import * as THREE from 'three';
import { EffectComposer } from 'three/examples/jsm/postprocessing/EffectComposer.js';
import { RenderPass } from 'three/examples/jsm/postprocessing/RenderPass.js';
import { UnrealBloomPass } from 'three/examples/jsm/postprocessing/UnrealBloomPass.js';
import { GlitchPass } from 'three/examples/jsm/postprocessing/GlitchPass.js';
import { OutputPass } from 'three/examples/jsm/postprocessing/OutputPass.js';

export class FlexarRenderer {
  constructor(canvas) {
    this.scene = new THREE.Scene();
    this.camera = new THREE.PerspectiveCamera(60, window.innerWidth / window.innerHeight, 0.1, 1000);
    this.camera.position.z = 30;

    this.renderer = new THREE.WebGLRenderer({ canvas, antialias: true });
    this.renderer.setSize(window.innerWidth, window.innerHeight);
    this.renderer.setPixelRatio(Math.min(window.devicePixelRatio, 2));

    this.initParticleSystem();
    this.initPostProcessing();
    this.handleResize();
  }

  initParticleSystem() {
    this.maxParticles = 100000;
    this.geometry = new THREE.BufferGeometry();
    this.positions = new Float32Array(this.maxParticles * 3);

    this.geometry.setAttribute('position', new THREE.BufferAttribute(this.positions, 3));

    const material = new THREE.PointsMaterial({
      color: 0x44aaff,
      size: 0.12,
      transparent: true,
      opacity: 0.8,
      blending: THREE.AdditiveBlending,
    });

    this.particles = new THREE.Points(this.geometry, material);
    this.scene.add(this.particles);
  }

  initPostProcessing() {
    // 1. Core Composer Pass
    this.composer = new EffectComposer(this.renderer);
    const renderPass = new RenderPass(this.scene, this.camera);
    this.composer.addPass(renderPass);

    // 2. Atmospheric Glow (UnrealBloomPass)
    const res = new THREE.Vector2(window.innerWidth, window.innerHeight);
    this.bloomPass = new UnrealBloomPass(res, 1.5, 0.4, 0.85); // strength, radius, threshold
    this.composer.addPass(this.bloomPass);

    // 3. Digital Glitch Pass
    this.glitchPass = new GlitchPass();
    this.glitchPass.enabled = false; // Toggleable via UI
    this.composer.addPass(this.glitchPass);

    // 4. Color Correction Output Pass
    const outputPass = new OutputPass();
    this.composer.addPass(outputPass);
  }

  updateParticles(particleData) {
    const posAttr = this.geometry.attributes.position;
    for (let i = 0; i < particleData.length / 4; i++) {
      posAttr.array[i * 3] = particleData[i * 4];
      posAttr.array[i * 3 + 1] = particleData[i * 4 + 1];
      posAttr.array[i * 3 + 2] = particleData[i * 4 + 2];
    }
    posAttr.needsUpdate = true;
  }

  render() {
    // Render through EffectComposer instead of direct WebGLRenderer
    this.composer.render();
  }

  handleResize() {
    window.addEventListener('resize', () => {
      const w = window.innerWidth;
      const h = window.innerHeight;
      this.camera.aspect = w / h;
      this.camera.updateProjectionMatrix();
      this.renderer.setSize(w, h);
      this.composer.setSize(w, h);
    });
  }
}
