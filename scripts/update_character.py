import json
import pathlib
import urllib.request
from datetime import datetime, timezone

URL = "https://iceflaredashboard.lanceshipley42.workers.dev/api/summary"
request = urllib.request.Request(URL, headers={"User-Agent": "wow-forever-char-dev/1.0", "Accept": "application/json"})
with urllib.request.urlopen(request, timeout=25) as response:
    if response.status != 200:
        raise RuntimeError(f"Unexpected HTTP status {response.status}")
    data = json.load(response)
character = data.get("character")
if not isinstance(character, dict) or character.get("name") != "Iceflare Theundying" or not isinstance(character.get("level"), int):
    raise ValueError("API response does not contain the expected Iceflare character data")
data["snapshotFetchedAt"] = datetime.now(timezone.utc).isoformat()
output = pathlib.Path("data/character.json")
output.parent.mkdir(parents=True, exist_ok=True)
output.write_text(json.dumps(data, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
print(f"Updated {character['name']} level {character['level']}")
