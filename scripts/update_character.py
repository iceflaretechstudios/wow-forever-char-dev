import json
import pathlib
import urllib.request
from datetime import datetime, timezone

SUMMARY_URL = "https://iceflaredashboard.lanceshipley42.workers.dev/api/summary"
ARMORY_URL = "https://iceflaredashboard.lanceshipley42.workers.dev/api/armory/forever/iceflare-theundying"
OUTPUT = pathlib.Path("data/character.json")


def fetch_json(url):
    request = urllib.request.Request(
        url, headers={"User-Agent": "wow-forever-char-dev/1.1", "Accept": "application/json"}
    )
    with urllib.request.urlopen(request, timeout=25) as response:
        if response.status != 200:
            raise RuntimeError(f"Unexpected HTTP status {response.status}")
        return json.load(response)


def find_money(value):
    """Read the IceCloud D1 copper balance from the character armory response."""
    if not isinstance(value, dict):
        return None
    character = value.get("character")
    if not isinstance(character, dict):
        return None
    raw = character.get("moneyCopper")
    if raw is None:
        return None
    try:
        copper = int(raw)
        if copper < 0:
            return None
    except (TypeError, ValueError):
        return None
    return {
        "gold": copper // 10000,
        "silver": (copper // 100) % 100,
        "copper": copper % 100,
        "totalCopper": copper,
        "goldLastSynced": character.get("goldLastSynced"),
    }


data = fetch_json(SUMMARY_URL)
character = data.get("character")
if not isinstance(character, dict) or character.get("name") != "Iceflare Theundying" or not isinstance(character.get("level"), int):
    raise ValueError("API response does not contain the expected Iceflare character data")

# The armory endpoint can include the IceCloud/D1 wallet, unlike /api/summary.
# Do not invent a balance or retain an old one as though it were fresh.
data.pop("money", None)
data.pop("moneySyncedAt", None)
try:
    armory = fetch_json(ARMORY_URL)
    money = find_money(armory)
    if money is None:
        print("Cloudflare armory responded, but no gold/silver/copper wallet was found")
    else:
        data["money"] = money
        data["moneySyncedAt"] = money["goldLastSynced"] or datetime.now(timezone.utc).isoformat()
        print(f"Gold synced: {money['gold']}g {money['silver']}s {money['copper']}c")
except (OSError, ValueError, json.JSONDecodeError) as exc:
    print(f"Gold sync unavailable; character snapshot will still update: {exc}")

data["snapshotFetchedAt"] = datetime.now(timezone.utc).isoformat()
OUTPUT.parent.mkdir(parents=True, exist_ok=True)
OUTPUT.write_text(json.dumps(data, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
print(f"Updated {character['name']} level {character['level']}")
