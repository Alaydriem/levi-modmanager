use amm_core::config::Budgets;
use amm_core::services::settings::ManagerConfigFile;

#[test]
fn a_read_clamps_hand_edited_budgets() {
    let text = "budgets {\n  event_queue = 100000000\n  io_queue = 0\n}\n";

    let (config, document) = ManagerConfigFile::read(text).unwrap_or_else(|error| panic!("{error}"));

    assert_eq!(config.budgets.event_queue, Budgets::MAX_EVENT_QUEUE);
    assert_eq!(config.budgets.io_queue, 1);
    assert_eq!(document.text(), text);
}

#[test]
fn unknown_language_reads_as_given() {
    let read = |text: &str| ManagerConfigFile::read(text).map(|(config, _)| config.language);

    // an unknown code is kept; each mod shows English for it
    assert_eq!(read("language = \"xx\"\n"), Ok("xx".to_owned()));
    assert_eq!(read("language = \"\"\n"), Ok(String::new()));
    assert_eq!(read("toggle_chord = \"Ctrl+Shift+F10\"\n"), Ok(String::new()));
    // hcl reads a number into a string field as a template text: an unknown code, so English
    assert_eq!(read("language = 3\n"), Ok("${3}".to_owned()));
}

#[test]
fn with_language_keeps_comments_and_other_attributes() {
    let text = "# the manager's own settings\ntoggle_chord = \"Ctrl+Shift+F10\"\n";
    let (_, document) = ManagerConfigFile::read(text).unwrap_or_else(|error| panic!("{error}"));

    let edited = ManagerConfigFile::with_language(&document, "en_XA");

    assert!(edited.text().contains("# the manager's own settings"), "{}", edited.text());
    assert!(edited.text().contains("toggle_chord"), "{}", edited.text());
    let (config, _) = ManagerConfigFile::read(&edited.text()).unwrap_or_else(|error| panic!("{error}"));
    assert_eq!(config.language, "en_XA");
}
