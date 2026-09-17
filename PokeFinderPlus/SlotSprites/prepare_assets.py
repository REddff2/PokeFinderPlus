"""Build-time generation of the DLL's pinned install recipe; no PNGs embedded."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import urllib.request
import zipfile

p = argparse.ArgumentParser()
p.add_argument('--host', type=Path, required=True)
p.add_argument('--vendor', type=Path, required=True)
p.add_argument('--output', type=Path, required=True)
p.add_argument('--download', action='store_true')
args = p.parse_args()
vendor, out = args.vendor, args.output
commit = json.loads((vendor / 'commit.json').read_text(encoding='utf-8'))['sha']
dex = json.loads((vendor / 'pokemon.json').read_text(encoding='utf-8'))
items = json.loads((vendor / 'item-map.json').read_text(encoding='utf-8'))
tree = {x['path'] for x in json.loads((vendor / 'tree.json').read_text(encoding='utf-8'))['tree'] if x['type'] == 'blob'}
# Explicit host form IDs, verified against forms_en.txt and gen-8 metadata.
forms = {201: list('abcdefghijklmnopqrstuvwxyz') + ['exclamation', 'question']}
for species, keys in {
    351: '$ sunny rainy snowy', 386: '$ attack defense speed',
    412: '$ sandy trash', 413: '$ sandy trash', 421: '$ sunshine',
    422: '$ east', 423: '$ east', 479: '$ heat wash frost fan mow',
    487: '$ origin', 492: '$ sky', 550: '$ blue-striped', 555: '$ zen',
    585: '$ summer autumn winter', 586: '$ summer autumn winter',
    641: '$ therian', 642: '$ therian', 645: '$ therian',
    646: '$ white black', 647: '$ resolute', 648: '$ pirouette',
    649: '$ douse shock burn chill',
}.items():
    forms[species] = keys.split()
host_forms = {}
for line in (args.host / 'Core/Resources/i18n/en/forms_en.txt').read_text(encoding='utf-8-sig').splitlines():
    species, form, name = line.split(',', 2)
    host_forms.setdefault(int(species), {})[int(form)] = name
assert all(set(v) == set(range(len(forms[k]))) for k, v in host_forms.items()), 'Host forms changed; inspect before packaging'

# Additional SwSh forms present in the frozen host's exported Raid tables.
# Numeric ordering checked against PKHeX FormConverter (Gen8) and PokéSprite metadata.
for s in (26,27,28,37,38,50,51,53,103,105): forms[s]=['$','alola']
forms[52]=['$','alola','galar']
for s in (77,78,79,83,110,122,144,145,146,199,222,263,264,554,562,618): forms[s]=['$','galar']
forms[555]=['$','zen','galar','galar-zen']
forms.update({678:['$','$'],710:['average','small','large','super'],711:['average','small','large','super'],
              744:['$','own-tempo'],745:['midday','midnight','dusk'],849:['amped','low-key'],
              854:['$','$'],855:['$','$'],876:['$','$'],892:['$','$']})
forms[869]=[key+'-plain' for key in ('vanilla-cream','ruby-cream','matcha-cream','mint-cream','lemon-cream',
                                  'salted-cream','ruby-swirl','caramel-swirl','rainbow-swirl')]
for s,keys in {
    25:'$ original-cap hoenn-cap sinnoh-cap unova-cap kalos-cap alola-cap partner-cap starter world-cap',
    80:'$ mega galar',384:'$ mega',414:'$ sandy trash',681:'$ blade',716:'$ active',
    718:'$ 10 10 50 complete',746:'$ school',778:'$ busted',800:'$ dusk dawn',801:'$ original',
    845:'$ gulping gorging',875:'$ noice',877:'$ hangry',888:'$ crowned',889:'$ crowned',
    890:'$ eternamax',893:'$ dada',898:'$ ice-rider shadow-rider',
}.items():forms[s]=keys.split()
types='$ fighting flying poison ground rock bug ghost steel fire water grass electric psychic ice dragon dark fairy'.split()
forms[493]=types.copy();forms[773]=types.copy()

manifest = {'schema': 1, 'source': 'msikma/pokesprite', 'commit': commit, 'pokemon': {}, 'items': {}}
assets = set()
for species in range(1, 899):
    entry = dex[f'{species:03}']
    slug = entry['slug']['eng']
    metadata = entry['gen-8']['forms']
    result = {}
    form_keys=dict(enumerate(forms.get(species, ['$'])))
    if species==493:
        gen4=types[:9]+['unknown']+types[9:17]
        form_keys.update({2000+n:key for n,key in enumerate(gen4)})
    if 'gmax' in metadata:
        for number in list(form_keys):
            form_keys[1000+number]='rapid-strike-gmax' if species==892 and number==1 else 'low-key-gmax' if species==849 and number==1 else 'gmax'
    for number, key in form_keys.items():
        seen = set()
        while metadata[key].get('is_alias_of'):
            assert key not in seen
            seen.add(key)
            key = metadata[key]['is_alias_of']
        stem = slug + ('' if key == '$' else '-' + key)
        variants = {}
        for shiny in (False, True):
            for female in (False, True):
                if female and not metadata[key].get('has_female', False):
                    continue
                path = 'pokemon-gen8/' + ('shiny/' if shiny else 'regular/') + ('female/' if female else '') + stem + '.png'
                if path in tree:
                    variants[('shiny' if shiny else 'normal') + ('Female' if female else '')] = path
                    assets.add(path)
        assert 'normal' in variants, (species, number, key)
        if species in (678,876) and number==1:
            variants['normal']=variants['normalFemale'];variants['shiny']=variants['shinyFemale']
        result[str(number)] = variants
    manifest['pokemon'][str(species)] = result

# Appletun shares Flapple's G-Max appearance (upstream pokesprite issue #110).
manifest['pokemon']['842']['1000']=manifest['pokemon']['841']['1000'].copy()
# These event entries carry a G-Max factor but have no G-Max transformation.
for species in (857,868):manifest['pokemon'][str(species)]['1000']=manifest['pokemon'][str(species)]['0'].copy()

host_items = {}
for line in (args.host / 'Core/Resources/i18n/en/items_en.txt').read_text(encoding='utf-8-sig').splitlines():
    number, name = line.split(',', 1)
    host_items[int(number)] = name
assert host_items[155] == 'Oran Berry' and items['item_0155'] == 'berry/oran'
missing = []
for number, name in host_items.items():
    if not number:
        continue
    relative = items.get(f'item_{number:04}')
    path = 'items/' + relative + '.png' if relative else ''
    if path in tree:
        manifest['items'][str(number)] = path
        assets.add(path)
    else:
        missing.append({'id': number, 'name': name})
out.mkdir(parents=True, exist_ok=True)
(out / 'manifest.json').write_text(json.dumps(manifest, ensure_ascii=False, indent=2), encoding='utf-8')
(out / 'unmapped-items.json').write_text(json.dumps(missing, indent=2), encoding='utf-8')
shutil.copyfile(vendor / 'license.md', out / 'POKESPRITE-LICENSE.md')
(out / 'ATTRIBUTION.txt').write_text(
    f'PokéSprite — https://github.com/msikma/pokesprite\nPinned commit: {commit}\n'
    'PokéSprite code/data is MIT licensed; see POKESPRITE-LICENSE.md.\n'
    'Pokémon and item artwork is copyright Nintendo / Creatures / GAME FREAK.\n'
    'Includes community-created shiny and other variants as described by PokéSprite.\n'
    'This installation contains a subset of pokemon-gen8 and items; artwork is unchanged.\n'
    'Generated manifest uses the upstream pokemon.json and item-map.json metadata.\n', encoding='utf-8')
archive = vendor / f'pokesprite-{commit}.zip'
if args.download and not archive.exists():
    urllib.request.urlretrieve(f'https://codeload.github.com/msikma/pokesprite/zip/{commit}', archive)
if archive.exists():
    hashes = {}
    files = {}
    with zipfile.ZipFile(archive) as z:
        for relative in sorted(assets | {'data/pokemon.json', 'data/item-map.json', 'license.md'}):
            data = z.read(f'pokesprite-{commit}/{relative}')
            local = 'sprites/' + relative if relative in assets else relative
            if relative == 'license.md': local = 'POKESPRITE-LICENSE.md'
            target = out / local
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(data)
            digest = hashlib.sha256(data).hexdigest()
            if relative in assets: hashes[relative] = digest
            files[local] = {'archive': relative, 'sha256': digest, 'size': len(data)}
    (out / 'asset-sha256.json').write_text(json.dumps(hashes, indent=2), encoding='utf-8')
    bootstrap = {'layout': 2, 'repo': 'msikma/pokesprite', 'commit': commit,
                 'archiveUrl': f'https://codeload.github.com/msikma/pokesprite/zip/{commit}',
                 'manifestText': (out / 'manifest.json').read_text(encoding='utf-8'),
                 'attributionText': (out / 'ATTRIBUTION.txt').read_text(encoding='utf-8').replace('PokéSprite —', 'PokéSprite by msikma —'),
                 'files': files}
    (out / 'bootstrap.json').write_text(json.dumps(bootstrap, ensure_ascii=False), encoding='utf-8')
print(f'{len(manifest["pokemon"])} species; {len(assets)} PNGs; {len(manifest["items"])} items; unmapped items: {missing}')
