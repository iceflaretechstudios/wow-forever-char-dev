export default {
  async fetch(request, env) {
    const path = new URL(request.url).pathname;
    const json = (value, status = 200) => new Response(JSON.stringify(value), {
      status, headers: {"Content-Type":"application/json; charset=utf-8","Cache-Control":"no-store"}
    });
    if (path === "/api/character") {
      try {
        if (!env.BLIZZARD_CLIENT_ID || !env.BLIZZARD_CLIENT_SECRET) throw new Error("Blizzard credentials are missing.");
        const auth = await fetch("https://oauth.battle.net/token", {
          method:"POST",
          headers:{"Authorization":"Basic "+btoa(env.BLIZZARD_CLIENT_ID+":"+env.BLIZZARD_CLIENT_SECRET),
                   "Content-Type":"application/x-www-form-urlencoded"},
          body:"grant_type=client_credentials"
        });
        if (!auth.ok) throw new Error("Blizzard authentication failed ("+auth.status+").");
        const token = (await auth.json()).access_token;
        const base = "https://us.api.blizzard.com/profile/wow/character/iceflare-theundying";
        const opts = {headers:{"Authorization":"Bearer "+token,"Accept":"application/json"}};
        const endpoints = [base,base+"/equipment",base+"/professions",base+"/statistics"];
        const responses = await Promise.all(endpoints.map(e => fetch(e+"?namespace=profile-forever-us&locale=en_US",opts)));
        const data = await Promise.all(responses.map(async (r) => r.ok ? r.json() : null));
        if (!responses[0].ok) throw new Error("Character lookup failed ("+responses[0].status+").");
        const c=data[0], e=data[1], p=data[2], stats=data[3];
        const equipped=e?.equipped_items??null;
        const iconIds=[...new Set((equipped??[]).map(x=>x.media?.id??x.item?.id).filter(Boolean))];
        const mediaResults=await Promise.all(iconIds.map(async id=>{
          const source=equipped.find(x=>(x.media?.id??x.item?.id)===id);
          const href=source?.media?.key?.href;
          if(!href || !href.startsWith("https://us.api.blizzard.com/"))return [id,null];
          try{
            const r=await fetch(href+(href.includes("?")?"&":"?")+"locale=en_US",opts);
            if(!r.ok)return [id,null];
            const m=await r.json();
            return [id,m.assets?.find(a=>a.key==="icon")?.value??m.assets?.[0]?.value??null];
          }catch{return [id,null]}
        }));
        const itemIcons=Object.fromEntries(mediaResults.filter(([,url])=>url&&url.startsWith("https://")));

        return json({
          character:{
            name:[c.name,c.surname].filter(Boolean).join(" "),
            level:c.level??null,
            characterClass:c.character_class?.name??null,
            faction:c.faction?.name??null,
            guild:c.guild?.name??null,
            averageItemLevel:c.average_item_level??null,
            ruleset:c.ruleset?.name??null
          },
          equipment:equipped,
          itemIcons,
          characterStats:stats,
          statisticsStatus:responses[3].status,
          professions:p ? {primaries:p.primaries??[],secondaries:p.secondaries??[]} : null,
          endpointStatus:{equipment:responses[1].status,professions:responses[2].status},
          updated:new Date().toISOString()
        });
      } catch(err) { return json({error:err.message},502); }
    }
    if (path!=="/") return new Response("Not found",{status:404});
    const html = `<!doctype html>
<html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Iceflare Dashboard v4</title>
<style>
*{box-sizing:border-box}body{margin:0;background:radial-gradient(circle at top,#24456c,#091321 70%);color:#eaf2ff;font:16px Arial,sans-serif;min-height:100vh}
main{max-width:1050px;margin:auto;padding:36px 18px}header{text-align:center;margin-bottom:32px}h1{color:#8cc6ff;font-size:clamp(30px,5vw,44px);margin:0 0 10px}
.sub,.label{color:#b3c9df}.card{background:#14243a;border:1px solid #36577a;border-radius:16px;padding:24px;margin:18px 0;box-shadow:0 10px 25px #0004}
h2{margin:0 0 20px;color:#d4e9ff}#name{font-size:28px;font-weight:bold}.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(155px,1fr));gap:12px;margin-top:20px}
.stat,.item{background:#1d3450;border:1px solid #355678;border-radius:10px;padding:15px;overflow-wrap:anywhere}
.label{font-size:13px;margin-bottom:8px}.value{font-size:20px;font-weight:bold}

/* Version 4.1: Blue average item level in both sections */
.ilvl-highlight{
  color:#00BFFF;
  font-size:28px;
  font-weight:bold;
  text-shadow:0 0 10px rgba(0,191,255,.5);
}

.items{display:grid;grid-template-columns:repeat(auto-fit,minmax(235px,1fr));gap:12px}
.item .slot{color:#b3c9df;font-size:13px;margin-bottom:7px}.item .item-name{font-weight:bold;font-size:17px}
.item .detail{font-size:13px;color:#d5e2f1;margin-top:6px;line-height:1.45}
.item{min-width:0}.item-head{display:flex;gap:12px;align-items:flex-start;margin-bottom:12px}
.item-icon{width:52px;height:52px;object-fit:cover;border:3px solid #9d9d9d;border-radius:8px;flex-shrink:0;box-shadow:0 0 8px currentColor}
.item-icon.quality-POOR{border-color:#9d9d9d;color:#9d9d9d}
.item-icon.quality-COMMON{border-color:#fff;color:#fff}
.item-icon.quality-UNCOMMON{border-color:#1eff00;color:#1eff00}
.item-icon.quality-RARE{border-color:#0070dd;color:#0070dd}
.item-icon.quality-EPIC{border-color:#a335ee;color:#a335ee}
.item-icon.quality-LEGENDARY{border-color:#ff8000;color:#ff8000}
.item-placeholder{width:52px;height:52px;border:2px solid #5d7894;border-radius:8px;display:grid;place-items:center;flex-shrink:0;font-size:26px}
.item .item-name{line-height:1.35}.item .slot{margin-bottom:5px}
.item .item-level{font-size:13px;color:#e3e9f2;margin-bottom:8px}
.item .binding{color:#e5d4a8}.item .bonus{color:#65ef89}
.item .price{color:#f5d38a}.item .weapon{color:#d8e5ff}
.quality-POOR{color:#9d9d9d}.quality-COMMON{color:#fff}.quality-UNCOMMON{color:#1eff00}
.quality-RARE{color:#409cff}.quality-EPIC{color:#a335ee}.quality-LEGENDARY{color:#ff8000}
.prof-symbol{font-size:24px;margin-right:8px}
.summary-grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(160px,1fr));gap:12px}
.meter{height:9px;background:#081a2d;border-radius:20px;overflow:hidden;margin-top:8px}
.meter-fill{height:100%;background:#62aaff}
.recipe-list{margin:10px 0 0;padding-left:20px;color:#cfe0f4;font-size:13px;line-height:1.7}
details{margin-top:12px}summary{cursor:pointer;color:#a8d5ff}
.note{font-size:12px;color:#9db7cf;margin-top:10px}

.prof{margin:18px 0}.prof h3{color:#a8d5ff;margin-bottom:10px}.prof-list{display:grid;grid-template-columns:repeat(auto-fit,minmax(210px,1fr));gap:12px}
button{background:#62aaff;color:#071321;border:0;border-radius:10px;padding:13px 22px;font-size:16px;font-weight:bold;cursor:pointer}
button:disabled{opacity:.6}#status{margin-top:14px;color:#b3c9df}a{color:#91c6ff}footer{text-align:center;padding:22px}
</style></head><body><main>
<header><h1>⚔️ Iceflare Dashboard</h1><div class="sub">World of Warcraft Forever · Version 4.1</div></header>
<section class="card"><div id="name">Iceflare Theundying</div><div class="sub">Live Character Information</div>
<div class="grid" id="stats"></div><p><button id="refresh" type="button">Refresh Character</button></p>
<div id="status" role="status" aria-live="polite">Loading character...</div></section>
<section class="card"><h2>📊 Character Statistics</h2><div class="grid" id="characterStats">Loading...</div><p class="note" id="statsNote"></p></section>
<section class="card"><h2>🛡 Equipment Summary</h2><div class="summary-grid" id="equipmentSummary">Loading...</div><p class="note">Armor and attributes here are bonuses from equipped items only, not full character statistics. Item level is supplied by Blizzard's character profile.</p></section>
<section class="card"><h2>⚔ Equipment</h2><div class="items" id="equipment">Loading...</div></section>
<section class="card"><h2>🛠 Professions</h2><div id="professions">Loading...</div></section>
<footer><a href="https://worldofwarcraft.blizzard.com/en-us/forever/us/armory/character/iceflare-theundying/" target="_blank" rel="noopener noreferrer">View Iceflare on Blizzard Armory</a><p>Character data provided by Blizzard API</p></footer>
</main><script>
const el=id=>document.getElementById(id);
const display=v=>v===null||v===undefined||v===""?"Not available":String(v);
const text=(tag,value,cls)=>{const e=document.createElement(tag);if(cls)e.className=cls;e.textContent=display(value);return e};

function renderStats(c){
  const stats=[["Level",c.level],["Class",c.characterClass],["Faction",c.faction],["Guild",c.guild],["Average Item Level",c.averageItemLevel],["Ruleset",c.ruleset]];
  el("name").textContent=display(c.name);

  /* Blue highlight for the character overview iLvl */
  el("stats").replaceChildren(...stats.map(([k,v])=>{
    const box=document.createElement("div");
    box.className="stat";
    box.append(
      text("div",k,"label"),
      text("div",v,k==="Average Item Level"?"value ilvl-highlight":"value")
    );
    return box;
  }));
}

function renderCharacterStats(stats,status){
  const box=el("characterStats");box.replaceChildren();
  const note=el("statsNote");
  if(!stats){box.append(text("p","Character statistics are unavailable for this character (HTTP "+status+")."));note.textContent="Equipment summaries below still work.";return}
  const fields=[
    ["Health",stats.health],["Power",stats.power],["Strength",stats.strength?.effective??stats.strength?.base],
    ["Agility",stats.agility?.effective??stats.agility?.base],
    ["Stamina",stats.stamina?.effective??stats.stamina?.base],
    ["Intellect",stats.intellect?.effective??stats.intellect?.base],
    ["Spirit",stats.spirit?.effective??stats.spirit?.base],
    ["Armor",stats.armor?.effective??stats.armor?.base],
    ["Attack Power",stats.attack_power],["Spell Power",stats.spell_power],
    ["Melee Crit",stats.melee_crit?.value],["Spell Crit",stats.spell_crit?.value]
  ].filter(([,v])=>v!==null&&v!==undefined&&typeof v!=="object");
  if(!fields.length){box.append(text("p","The statistics endpoint responded, but returned no supported fields."));return}
  for(const [name,value] of fields){const card=document.createElement("div");card.className="stat";card.append(text("div",name,"label"),text("div",value,"value"));box.append(card)}
  note.textContent="Live character values reported by Blizzard.";
}

function renderSummary(items,c){
  const box=el("equipmentSummary");box.replaceChildren();
  if(!items){box.append(text("p","Equipment data unavailable."));return}
  const bonuses={};let armor=0,sell=0,low=[];
  for(const item of items){
    armor+=Number(item.armor?.value??0);
    sell+=Number(item.sell_price?.value??0);
    for(const stat of item.stats??[]){
      const key=stat.type?.name??"Other";
      bonuses[key]=(bonuses[key]??0)+Number(stat.value??0);
    }
    if(item.durability?.value!=null){
      const match=String(item.durability.display_string??"").split("/");
      const max=match.length>1?Number(match[1].trim()):null;
      if(max&&item.durability.value/max<0.3)low.push(item.name);
    }
  }
  const fields=[
    ["Average Item Level (iLvl)",c.averageItemLevel],
    ["Equipped Items",items.length],
    ["Armor from Gear",armor],
    ["Gear Vendor Value",priceString({value:sell})?.replace("Sell Price: ","")],
    ...Object.entries(bonuses).map(([k,v])=>[k+" from Gear","+"+v])
  ];

  /* Matching blue highlight in the Equipment Summary */
  for(const [label,value] of fields){
    const card=document.createElement("div");
    card.className="stat";
    card.append(
      text("div",label,"label"),
      text("div",value,label==="Average Item Level (iLvl)"?"value ilvl-highlight":"value")
    );
    box.append(card);
  }

  if(low.length){const warning=text("p","Low durability: "+low.join(", "));warning.style.color="#ffd27b";box.append(warning)}
}

function addDetail(card,value,cls="detail"){if(value!==null&&value!==undefined&&value!=="")card.append(text("div",value,cls))}
function priceString(price){
  if(!price)return null;
  const d=price.display_strings;
  if(d)return "Sell Price: "+[["gold","g"],["silver","s"],["copper","c"]].filter(([k])=>Number(d[k])>0).map(([k,u])=>d[k]+u).join(" ") || "Sell Price: 0c";
  const v=price.value;if(v===undefined)return null;
  return "Sell Price: "+[Math.floor(v/10000)+"g",Math.floor(v%10000/100)+"s",(v%100)+"c"].join(" ");
}
function renderEquipment(items,status,icons={}){
  const box=el("equipment");box.replaceChildren();
  if(!items){box.append(text("p","Equipment unavailable (HTTP "+status+")."));return}
  if(!items.length){box.append(text("p","No equipped items returned."));return}
  for(const item of items){
    const card=document.createElement("article");card.className="item";
    const head=document.createElement("div");head.className="item-head";
    const icon=icons[item.media?.id??item.item?.id];
    if(icon){
      const img=document.createElement("img");img.className="item-icon quality-"+(item.quality?.type??"COMMON").toUpperCase();img.src=icon;
      img.alt="";img.loading="lazy";img.referrerPolicy="no-referrer";
      img.onerror=()=>{const fallback=text("span","⚔","item-placeholder");img.replaceWith(fallback)};
      head.append(img);
    }else head.append(text("span","⚔","item-placeholder"));
    const title=document.createElement("div");
    title.append(text("div",item.slot?.name??"Equipment","slot"));
    const quality=(item.quality?.type??"COMMON").toUpperCase();
    title.append(text("div",item.name??"Unnamed item","item-name quality-"+quality));
    head.append(title);card.append(head);
    addDetail(card,item.level?.display_string??(item.level?.value!=null?"Item Level "+item.level.value:null),"item-level");
    addDetail(card,item.binding?.name,"detail binding");
    const type=[item.inventory_type?.name,item.item_subclass?.name].filter(Boolean);
    if(type.length)addDetail(card,type.join(" · "));
    addDetail(card,item.armor?.display?.display_string??(item.armor?.value!=null?item.armor.value+" Armor":null));
    if(item.weapon){
      addDetail(card,item.weapon.damage?.display_string,"detail weapon");
      addDetail(card,item.weapon.attack_speed?.display_string,"detail weapon");
      addDetail(card,item.weapon.dps?.display_string,"detail weapon");
    }
    for(const stat of item.stats??[]){
      const str=stat.display?.display_string??((stat.value>=0?"+":"")+stat.value+" "+(stat.type?.name??"Stat"));
      addDetail(card,str,stat.is_equip_bonus?"detail bonus":"detail");
    }
    addDetail(card,item.requirements?.level?.display_string);
    addDetail(card,item.durability?.display_string);
    addDetail(card,priceString(item.sell_price),"detail price");
    box.append(card);
  }
}
function professionName(entry){
  return entry.profession?.name??entry.name??entry.profession?.type??"Unknown profession";
}
function renderProfessions(p,status){
  const box=el("professions");box.replaceChildren();
  if(!p){box.append(text("p","Professions unavailable (HTTP "+status+")."));return}
  for(const [title,entries] of [["Primary Professions",p.primaries],["Secondary Professions",p.secondaries]]){
    const group=document.createElement("div");group.className="prof";
    group.append(text("h3",title));
    const list=document.createElement("div");list.className="prof-list";
    if(!entries.length)list.append(text("p","None returned."));
    for(const entry of entries){
      const card=document.createElement("div");card.className="item";
      const icons={"Skinning":"🗡️","Herbalism":"🌿","Cooking":"🍳","Fishing":"🎣","First Aid":"🩹"};
      card.append(text("span",icons[professionName(entry)]??"🛠️","prof-symbol"));
      card.append(text("span",professionName(entry),"item-name"));
      const tiers=entry.tiers??[];
      if(tiers.length){
        for(const tier of tiers){
          const skill=tier.skill_points??null,max=tier.max_skill_points??null;
          const label=(tier.tier?.name??"Skill")+": "+display(skill)+(max!==null?" / "+max:"");
          card.append(text("div",label,"detail"));
        }
      } else if(entry.skill_points!==undefined) {
        card.append(text("div","Skill: "+entry.skill_points+(entry.max_skill_points!==undefined?" / "+entry.max_skill_points:""),"detail"));
      }
      const recipes=[...new Map(tiers.flatMap(t=>t.known_recipes??[]).map(r=>[r.id??r.name,r])).values()];
      if(recipes.length){
        const detail=document.createElement("details");
        const heading=document.createElement("summary");heading.textContent="Known recipes ("+recipes.length+")";detail.append(heading);
        const ul=document.createElement("ul");ul.className="recipe-list";
        for(const recipe of recipes){const li=document.createElement("li");li.textContent=recipe.name??"Unnamed recipe";ul.append(li)}
        detail.append(ul);card.append(detail);
      }
      if(tiers.length){
        const t=tiers[0],max=Number(t.max_skill_points??0),current=Number(t.skill_points??0);
        if(max>0){const meter=document.createElement("div");meter.className="meter";const fill=document.createElement("div");fill.className="meter-fill";fill.style.width=Math.min(100,Math.max(0,current/max*100))+"%";meter.append(fill);card.append(meter)}
      }
      list.append(card);
    }
    group.append(list);box.append(group);
  }
}
async function refresh(){
  const button=el("refresh");button.disabled=true;el("status").textContent="Loading from Blizzard...";
  try{
    const response=await fetch("/api/character",{cache:"no-store"});
    const data=await response.json();
    if(!response.ok)throw new Error(data.error??"Character data unavailable");
    renderStats(data.character);renderCharacterStats(data.characterStats,data.statisticsStatus);renderSummary(data.equipment,data.character);renderEquipment(data.equipment,data.endpointStatus.equipment,data.itemIcons);
    renderProfessions(data.professions,data.endpointStatus.professions);
    el("status").textContent="Last checked: "+new Date(data.updated).toLocaleString();
  }catch(err){el("status").textContent="Error: "+err.message}
  finally{button.disabled=false}
}
el("refresh").addEventListener("click",refresh);refresh();
</script></body></html>`;
    return new Response(html,{headers:{
      "Content-Type":"text/html; charset=utf-8",
      "Cache-Control":"no-store",
      "X-Content-Type-Options":"nosniff",
      "Content-Security-Policy":"default-src 'none'; script-src 'unsafe-inline'; style-src 'unsafe-inline'; connect-src 'self'; img-src https: data:; base-uri 'none'; frame-ancestors https://dash.cloudflare.com"
    }});
  }
};