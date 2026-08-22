use wasm_bindgen::prelude::*;

#[wasm_bindgen]
pub struct NodeGraphChain {
    particle_buffer: Vec<f32>,
    count: usize,
    pub speed: f32,
    pub spread: f32,
}

#[wasm_bindgen]
impl NodeGraphChain {
    #[wasm_bindgen(constructor)]
    pub fn new(max_particles: usize) -> Self {
        let buffer = vec![0.0; max_particles * 4];

        let mut chain = Self {
            particle_buffer: buffer,
            count: max_particles,
            speed: 1.0,
            spread: 30.0,
        };
        chain.reset_positions();
        chain
    }

    pub fn reset_positions(&mut self) {
        for i in 0..self.count {
            let idx = i * 4;
            self.particle_buffer[idx] = (js_sys::Math::random() as f32 - 0.5) * self.spread;
            self.particle_buffer[idx + 1] = (js_sys::Math::random() as f32 - 0.5) * self.spread;
            self.particle_buffer[idx + 2] = (js_sys::Math::random() as f32 - 0.5) * self.spread;
            self.particle_buffer[idx + 3] = 0.8;
        }
    }

    pub fn update(&mut self, _delta_time: f32, elapsed_time: f32) {
        for i in 0..self.count {
            let idx = i * 4;
            let offset = i as f32 * 0.05;
            let wave = (elapsed_time * self.speed + offset).sin() * 0.02 * self.speed;
            self.particle_buffer[idx] += wave;
            self.particle_buffer[idx + 1] += wave * 0.5;
        }
    }

    pub fn set_speed(&mut self, speed: f32) {
        self.speed = speed;
    }

    pub fn set_spread(&mut self, spread: f32) {
        self.spread = spread;
        self.reset_positions();
    }

    pub fn get_particle_buffer_ptr(&self) -> *const f32 {
        self.particle_buffer.as_ptr()
    }

    pub fn get_particle_buffer_size(&self) -> usize {
        self.particle_buffer.len()
    }
}
