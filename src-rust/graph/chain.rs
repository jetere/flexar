use super::traits::EffectNode;
use wasm_bindgen::prelude::*;

#[wasm_bindgen]
pub struct NodeGraphChain {
    nodes: Vec<Box<dyn EffectNode>>,
    particle_buffer: Vec<f32>, // Flat Float32 array [x, y, z, opacity, ...]
}

#[wasm_bindgen]
impl NodeGraphChain {
    #[wasm_bindgen(constructor)]
    pub fn new(max_particles: usize) -> Self {
        // Allocate x, y, z, opacity for each particle
        let particle_buffer = vec![0.0; max_particles * 4];
        Self {
            nodes: Vec::new(),
            particle_buffer,
        }
    }

    pub fn update(&mut self, delta_time: f32, elapsed_time: f32) {
        for node in self.nodes.iter_mut() {
            if node.is_enabled() {
                node.process(delta_time, elapsed_time, &mut self.particle_buffer);
            }
        }
    }

    // Expose raw particle buffer pointer directly to Node/WebGL zero-copy
    pub fn get_particle_buffer_ptr(&self) -> *const f32 {
        self.particle_buffer.as_ptr()
    }

    pub fn get_particle_buffer_size(&self) -> usize {
        self.particle_buffer.len()
    }
}
