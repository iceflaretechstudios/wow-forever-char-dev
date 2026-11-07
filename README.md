# WoW Forever Char Dev

A public, read-only GitHub snapshot of Iceflare Theundying's WoW Forever character data.

## Setup

1. Create a **public** GitHub repository named `wow-forever-char-dev` under `iceflaretechstudios`.
2. Upload the contents of this ZIP (not the outer folder) to the repository, preserving `.github/workflows/update-character.yml`.
3. In repository **Settings > Actions > General**, set **Workflow permissions** to **Read and write permissions**, and save.
4. Open **Actions > Update WoW Forever character > Run workflow** to create the first snapshot.
5. Check `data/character.json` after the workflow succeeds. The workflow runs every 30 minutes (GitHub schedules can be delayed).
6. The public raw JSON URL will be:
   https://raw.githubusercontent.com/iceflaretechstudios/wow-forever-char-dev/main/data/character.json

If your default branch is not `main`, adjust the raw URL accordingly.

This repository contains only publicly accessible character information, not Blizzard API secrets. Do not upload credentials. If the workflow fails, inspect the Actions logs; the Cloudflare endpoint must be reachable from GitHub Actions.

**Note:** ChatGPT's ability to retrieve the raw GitHub URL still needs independent verification.
