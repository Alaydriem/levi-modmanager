use amm_client::link::{FrameHandler, FrameInputView, FrameReply};
use amm_core::structs::frame::CursorShape;
use egui::{Context, Rect};

/// The frame pass of a mod that draws nothing.
pub struct IdleUi;

impl FrameHandler for IdleUi {
    fn frame(&mut self, _ctx: &Context, _input: &FrameInputView) -> FrameReply {
        FrameReply { cursor: CursorShape::Default, layer_rect: Rect::NOTHING }
    }
}
