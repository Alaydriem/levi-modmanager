use amm_client::link::{FrameHandler, FrameInputView, FrameReply};
use amm_core::structs::frame::{CursorShape, LayerKind};
use egui::{Area, Context, Id, Rect, RichText, Sense, TextEdit, Ui, Window, pos2, vec2};
use levi_i18n::text::Text;
use levi_ui::LeviUiMessage;
use levi_ui::draw::TypeSize;
use levi_ui::font::FontFace;
use levi_ui::tokens::rule::Elevation;
use levi_ui::tokens::{ColorToken, SpacingToken, color, spacing};
use levi_ui::widget::{ButtonKind, Surface, SurfaceLevel, TextButton, Toggle};

use crate::structs::{DemoCell, DemoState};

/// The demo's egui pass: the card on the Hud layer and the window on the Overlay layer. It reads what the mod
/// service decided from the shared state and draws only the content of the layer that each call draws.
pub struct DemoUi {
    cell: DemoCell,
}

impl DemoUi {
    const MARK: f32 = 8.0;
    const MARK_GAP: f32 = 3.0;
    const MARKS_PER_LINE: usize = 16;
    const BAR: (f32, f32) = (150.0, 6.0);
    const WINDOW_POS: (f32, f32) = (120.0, 120.0);
    const NAME_WIDTH: f32 = 160.0;

    pub fn new(cell: DemoCell) -> Self {
        Self { cell }
    }

    fn paint(ctx: &Context, layer: LayerKind, state: &mut DemoState) -> FrameReply {
        match layer {
            LayerKind::Hud => {
                let rect = if state.card_visible() { Self::card(ctx, state) } else { Rect::NOTHING };
                FrameReply { cursor: CursorShape::Default, layer_rect: rect }
            }
            LayerKind::Overlay => match Self::window(ctx, state) {
                Some(rect) => FrameReply { cursor: CursorShape::Arrow, layer_rect: rect },
                None => FrameReply { cursor: CursorShape::Default, layer_rect: Rect::NOTHING },
            },
        }
    }

    fn card(ctx: &Context, state: &DemoState) -> Rect {
        let config = state.config();
        let (align, offset) = config.card.corner.anchor();
        let fill = <color::Card as ColorToken>::VALUE.gamma_multiply(config.card.opacity.clamp(0.0, 1.0) as f32);
        let shown = Area::new(Id::unique("demo.card")).anchor(align, offset).interactable(false).show(ctx, |ui| {
            Surface::frame(SurfaceLevel::Card, Elevation::Floating).fill(fill).show(ui, |ui| {
                ui.label(
                    RichText::new("Demo").font(FontFace::Semibold.id(TypeSize::Body.points())).color(state.accent()),
                );
                ui.label(format!("Rows {}", config.card.rows));
                ui.label(format!("Opacity {:.2}", config.card.opacity));
                if !config.card.compact {
                    Self::marks(ui, config.card.rows, state);
                    Self::bar(ui, config.card.opacity, state);
                }
                ui.label(RichText::new(state.camera_line()).color(<color::TextDim as ColorToken>::VALUE));
                if !state.name().is_empty() {
                    ui.label(
                        RichText::new(format!("Name {}", state.name())).color(<color::TextDim as ColorToken>::VALUE),
                    );
                }
                if !state.screenshot_line().is_empty() {
                    ui.label(RichText::new(state.screenshot_line()).color(<color::TextDim as ColorToken>::VALUE));
                }
                if state.show_last_event() {
                    ui.label(
                        RichText::new(format!("Last event {}", state.last_event()))
                            .color(<color::TextDim as ColorToken>::VALUE),
                    );
                }
            });
        });
        shown.response.rect
    }

    fn marks(ui: &mut Ui, rows: i64, state: &DemoState) {
        let mut left = usize::try_from(rows).unwrap_or(0);
        while left > 0 {
            let line = left.min(Self::MARKS_PER_LINE);
            let width = line as f32 * (Self::MARK + Self::MARK_GAP);
            let (rect, _) = ui.allocate_exact_size(vec2(width, Self::MARK), Sense::hover());
            for i in 0..line {
                let x = rect.left() + i as f32 * (Self::MARK + Self::MARK_GAP);
                let mark = Rect::from_min_size(pos2(x, rect.top()), vec2(Self::MARK, Self::MARK));
                ui.painter().rect_filled(mark, 2.0, state.accent());
            }
            left -= line;
        }
    }

    fn bar(ui: &mut Ui, fraction: f64, state: &DemoState) {
        let (rect, _) = ui.allocate_exact_size(vec2(Self::BAR.0, Self::BAR.1), Sense::hover());
        ui.painter().rect_filled(rect, 3.0, <color::BgField as ColorToken>::VALUE);
        let filled = Rect::from_min_size(rect.min, vec2(rect.width() * fraction.clamp(0.0, 1.0) as f32, rect.height()));
        ui.painter().rect_filled(filled, 3.0, state.accent());
    }

    fn window(ctx: &Context, state: &mut DemoState) -> Option<Rect> {
        if !state.window_open() {
            return None;
        }
        let mut open = true;
        let mut show_last_event = state.show_last_event();
        let mut close = false;
        let shown = Window::new("Demo window")
            .open(&mut open)
            .collapsible(false)
            .resizable(false)
            .default_pos(pos2(Self::WINDOW_POS.0, Self::WINDOW_POS.1))
            .show(ctx, |ui| {
                ui.label("This window holds the Overlay lease and asks for the cursor.");
                ui.add_space(<spacing::S as SpacingToken>::VALUE);
                ui.horizontal(|ui| {
                    Toggle::new(&mut show_last_event).show(ui);
                    ui.label("Show the last event on the card");
                });
                ui.add_space(<spacing::S as SpacingToken>::VALUE);
                ui.horizontal(|ui| {
                    ui.label("Name");
                    ui.add(TextEdit::singleline(state.name_mut()).desired_width(Self::NAME_WIDTH));
                });
                ui.add_space(<spacing::S as SpacingToken>::VALUE);
                close = TextButton::new(ButtonKind::Secondary, Text::message(LeviUiMessage::Close)).show(ui).clicked();
            });
        state.set_show_last_event(show_last_event);
        if close || !open {
            state.set_window_open(false);
        }
        shown.map(|window| window.response.rect)
    }
}

impl FrameHandler for DemoUi {
    fn frame(&mut self, ctx: &Context, input: &FrameInputView) -> FrameReply {
        self.cell.with(|state| Self::paint(ctx, input.layer(), state))
    }
}
