id           = "alaydriem-demo-b"
display_name = "Demo B"
version      = "0.1.0"
author       = "Alaydriem"
config_path  = "config.hcl"

claim "Camera" {
  mode   = "Exclusive"
  reason = "Holds the camera while its key is toggled on, so the demo mod is suspended"
}
claim "WorldDraw" {
  mode   = "Shared"
  reason = "Stands in for a mod that draws in the world"
}

keybind "toggle_camera" {
  label       = "Take or release the camera"
  default     = "Alt+1"
  capability  = "WorldDraw"
  scope       = "Global"
  config_path = "keys.toggle_camera"
}

section "keys" {
  title = "Keys"

  field "keys.toggle_camera" {
    label   = "Take or release the camera"
    kind    = "chord"
    default = "Alt+1"
    apply   = "Live"
  }
}
