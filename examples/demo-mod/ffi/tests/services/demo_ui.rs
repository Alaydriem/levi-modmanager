use amm_client::link::{FrameHandler, FrameInputView};
use amm_core::structs::frame::{FrameInputData, LayerKind};
use demo_ffi::services::DemoUi;
use demo_ffi::structs::DemoCell;
use egui::{Context, RawInput};

fn run(ui: &mut DemoUi, layer: LayerKind) -> egui::Rect {
    let ctx = Context::default();
    let allowed_layers = match layer {
        LayerKind::Hud => FrameInputData::LAYER_HUD,
        LayerKind::Overlay => FrameInputData::LAYER_OVERLAY,
    };
    let data = FrameInputData {
        width: 1280,
        height: 720,
        pixels_per_point: 1.0,
        time_secs: 0.0,
        events: &[],
        allowed_layers,
        input_flags: 0,
    };
    let input = FrameInputView::new(data, layer);
    let raw = RawInput {
        screen_rect: Some(egui::Rect::from_min_size(egui::Pos2::ZERO, egui::vec2(1280.0, 720.0))),
        ..RawInput::default()
    };
    let mut rect = egui::Rect::NOTHING;
    for _ in 0..2 {
        let mut output = ctx.run_pass(raw.clone(), |ctx| rect = ui.frame(ctx, &input).layer_rect);
        output.textures_delta.clear();
    }
    rect
}

#[test]
fn the_card_draws_only_while_visible_and_only_on_the_hud_layer() {
    let cell = DemoCell::new();
    let mut ui = DemoUi::new(cell.clone());

    cell.with(|state| state.set_card_visible(true));
    assert!(run(&mut ui, LayerKind::Hud).is_positive(), "a visible card is drawn on the hud layer");
    assert!(!run(&mut ui, LayerKind::Overlay).is_positive(), "the overlay layer does not show the card");

    cell.with(|state| state.set_card_visible(false));
    assert!(!run(&mut ui, LayerKind::Hud).is_positive(), "hidden card draws nothing");
}

#[test]
fn the_window_draws_only_while_open_and_only_on_the_overlay_layer() {
    let cell = DemoCell::new();
    let mut ui = DemoUi::new(cell.clone());

    cell.with(|state| state.set_window_open(true));
    assert!(!run(&mut ui, LayerKind::Hud).is_positive(), "the hud layer cannot show the window");
    assert!(run(&mut ui, LayerKind::Overlay).is_positive());

    cell.with(|state| state.set_window_open(false));
    assert!(!run(&mut ui, LayerKind::Overlay).is_positive(), "a closed window draws nothing");
}
