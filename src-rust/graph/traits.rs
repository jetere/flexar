use std::collections::HashMap;

pub trait EffectNode {
    fn id(&self) -> &str;
    fn name(&self) -> &str;
    fn is_enabled(&self) -> bool;
    fn set_enabled(&mut self, enabled: bool);
    fn set_param(&mut self, key: &str, value: f32);

    // Process input buffer data and apply node transformation
    fn process(&mut self, delta_time: f32, elapsed_time: f32, input_particles: &mut Vec<f32>);
}
