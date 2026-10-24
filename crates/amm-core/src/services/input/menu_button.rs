use crate::structs::frame::MenuScreen;

/// The manager's own "Mods" button: whether the config allows it, which game screen is on top, and
/// where the manager's last pass drew it in physical pixels. It shows only while it is allowed, the
/// start or the pause screen is on top and the panel is closed. Fixed in size.
pub struct MenuButton {
    allowed: bool,
    screen: MenuScreen,
    rect: Option<[f32; 4]>,
}

impl MenuButton {
    pub fn new(allowed: bool) -> Self {
        Self { allowed, screen: MenuScreen::None, rect: None }
    }

    pub fn screen(&self) -> MenuScreen {
        self.screen
    }

    pub fn allowed(&self) -> bool {
        self.allowed
    }

    /// The screen on top in the frame the host is about to draw. Any other screen forgets the drawn
    /// rect, so play and other menus never route to a button that is gone.
    pub fn set_screen(&mut self, screen: MenuScreen) {
        self.screen = screen;
        if screen == MenuScreen::None {
            self.rect = None;
        }
    }

    pub fn visible(&self, panel_open: bool) -> bool {
        self.allowed && self.screen != MenuScreen::None && !panel_open
    }

    /// Where the last pass drew the button, or `None` when it drew none.
    pub fn place(&mut self, rect: Option<[f32; 4]>) {
        self.rect = rect;
    }

    /// The drawn rect while the button shows.
    pub fn rect(&self, panel_open: bool) -> Option<[f32; 4]> {
        self.rect.filter(|_| self.visible(panel_open))
    }

    /// True when the button shows and `(x, y)` is inside its drawn rect.
    pub fn contains(&self, panel_open: bool, (x, y): (f32, f32)) -> bool {
        self.rect(panel_open)
            .is_some_and(|[min_x, min_y, max_x, max_y]| x >= min_x && x < max_x && y >= min_y && y < max_y)
    }
}
