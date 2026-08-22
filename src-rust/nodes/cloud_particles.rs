// Volumetric cloud math & particle buffer
use crate::graph::traits::EffectNode;
use glam::Vec3;

pub struct CloudParticleNode {
    id: String,
    enabled: bool,
    density: f32,
    speed: f32,
}

impl CloudParticleNode {
    pub fn new(id: &str) -> Self {
        Self {
            id: id.to_string(),
            enabled: true,
            density: 100.0,
            speed: 0.5,
        }
    }
}

impl EffectNode for CloudParticleNode {
    fn id(&self) -> &str {
        &self.id
    }
    fn name(&self) -> &str {
        "Cloud Particles"
    }
    fn is_enabled(&self) -> bool {
        self.enabled
    }
    fn set_enabled(&mut self, enabled: bool) {
        self.enabled = enabled;
    }

    fn set_param(&mut self, key: &str, value: f32) {
        match key {
            "density" => self.density = value,
            "speed" => self.speed = value,
            _ => {}
        }
    }

    fn process(&mut self, _dt: f32, elapsed: f32, buffer: &mut Vec<f32>) {
        let count = buffer.len() / 4;
        for i in 0..count {
            let idx = i * 4;
            // Generate deterministic cloud drift noise
            let phase = elapsed * self.speed + (i as f32 * 0.1);
            buffer[idx] += (phase.sin() * 0.01); // X shift
            buffer[idx + 1] += (phase.cos() * 0.005); // Y drift
            buffer[idx + 3] = 0.7; // Opacity
        }
    }
}
