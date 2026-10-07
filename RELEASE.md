Publishing WiFiManagerCustom

Steps to publish to Arduino Library Manager:
1. Ensure `library.json` contains all required fields: name, version, author, sentence, paragraph, url, category, architectures, license, examples.
2. Tag a release on GitHub with the version (e.g., `v1.0.0`) and push a release zip matching the `library.json` version.
3. Open an issue at Arduino Library Manager repository requesting the library to be added and include the GitHub repo URL.
4. Wait for the Arduino Library Manager process to accept and index your library.

Notes:
- The library already includes `examples/` and `library.properties`.
- Ensure the library compiles for `esp32` boards and dependencies are accurate.
