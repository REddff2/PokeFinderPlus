"""Developer-only: exact localized names for controls that have no ID role."""
import json
from pathlib import Path
host=Path('C:/PokeFinderDev/PokeFinderPlus-src/Core/Resources/i18n')
names={}
def add(name, identity):
    if name in names and names[name]!=identity: names[name]=None
    else: names[name]=identity
for locale in host.iterdir():
    species=(locale/f'species_{locale.name}.txt').read_text(encoding='utf-8-sig').splitlines()
    for n,name in enumerate(species[:898],1): add(name,[n,0])
    for line in (locale/f'forms_{locale.name}.txt').read_text(encoding='utf-8-sig').splitlines():
        s,f,label=line.split(',',2);s,f=int(s),int(f)
        if s<=898 and label: add(f'{species[s-1]} ({label})',[s,f])
out=Path(__file__).parent/'resources/control-names.json'
out.write_text(json.dumps({k:v for k,v in names.items() if v is not None},ensure_ascii=False,separators=(',',':')),encoding='utf-8')
print(len(names),'localized names inspected')
games={}
for locale in host.iterdir():
    values=(locale/f'games_{locale.name}.txt').read_text(encoding='utf-8-sig').splitlines()
    for index,key in [(14,'Black 2'),(15,'White 2'),(24,'Sword'),(25,'Shield'),(26,'Brilliant Diamond'),(27,'Shining Pearl')]:games[values[index]]=key
(Path(__file__).parent/'resources/control-games.json').write_text(json.dumps(games,ensure_ascii=False),encoding='utf-8')
