use egui::{Response, Sense, Ui};
use levi_i18n::text::Text;
use levi_ui::draw::{Draw, TextFit, TextStyle, TypeSize};
use levi_ui::font::FontFace;
use levi_ui::text::UiText;
use levi_ui::tokens::{ColorToken, color};

/// A faint line of help under a table or a page, such as "Click a keybind, then press its new chord.
/// Esc cancels." It wraps at the width it is given.
pub struct PanelHint;

impl PanelHint {
    const STYLE: TextStyle = TextStyle::new(TypeSize::Help, FontFace::Regular);

    pub fn show(ui: &mut Ui, text: &Text) -> Response {
        Self::show_in(ui, text, <color::TextFaint as ColorToken>::VALUE)
    }

    /// The same line in `--color-text-dim`, for an intro above a table.
    pub fn show_dim(ui: &mut Ui, text: &Text) -> Response {
        Self::show_in(ui, text, <color::TextDim as ColorToken>::VALUE)
    }

    fn show_in(ui: &mut Ui, text: &Text, colour: egui::Color32) -> Response {
        let words = UiText::resolve(ui.ctx(), text);
        let galley = Draw::galley(ui.painter(), &words, Self::STYLE, colour, TextFit::Wrap(ui.available_width()));
        let (rect, response) = ui.allocate_exact_size(galley.size(), Sense::hover());
        ui.painter().galley(rect.min, galley, colour);
        response
    }
}
