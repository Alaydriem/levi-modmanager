use std::sync::Arc;

use levi_i18n::localize::Localizer;
use levi_i18n::message::Message;
use levi_i18n::service::{LocaleSetting, LocalizationService};
use levi_ui::LeviUiMessage;
use levi_ui::text::LeviUiPacks;

use crate::AmmMessage;
use crate::text::AmmPacks;

/// The panel's words in one language: its own packs and the levi-ui widgets' packs.
pub struct PanelLocale;

impl PanelLocale {
    /// `code` is the manager's language code: empty for English, `en_XA` for the pseudo-locale.
    pub fn localizer(code: &str) -> Arc<Localizer> {
        Arc::new(Self::service().load(&LocaleSetting::from_code(code)))
    }

    /// The languages the panel can show: English (the empty code) first, then each pack amm-ui embeds.
    pub fn languages() -> Vec<String> {
        let mut codes = vec![String::new()];
        codes.extend(Self::service().available(AmmMessage::DOMAIN));
        codes
    }

    fn service() -> LocalizationService {
        LocalizationService::new(vec![
            (LeviUiMessage::DOMAIN, LeviUiPacks::SOURCE),
            (AmmMessage::DOMAIN, AmmPacks::SOURCE),
        ])
    }
}
