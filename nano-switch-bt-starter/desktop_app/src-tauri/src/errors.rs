use thiserror::Error;

#[derive(Debug, Error)]
pub enum AppError {
    #[error("too many simultaneous pressed keys: {0}")]
    TooManyPressedKeys(usize),
    #[error("unsupported mouse button index: {0}")]
    UnsupportedMouseButton(u8),
}
