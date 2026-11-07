# Example of usage `libopusfile` as simple files player wrapped as `loaddsp` module.
## Command-line options
- `-f` - input Opus-encoded file.
- `-a` - amplitude modifier of output audio signal.
- `-v` - volume modifier of output audio signal.
- `-r` - repeat control. See details below.

### Repeat option values
Negative values enable infinite looping. Positive values plays once time plus repeats N time over. Zero means no repeating and playing only once time.

## Dependencies
- [loaddsp](https://codeberg.org/RomanDonw/loaddsp).
- libopusfile.
