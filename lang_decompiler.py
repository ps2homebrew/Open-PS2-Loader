import os
import yaml

def represent_str(self, data):
    tag = u'tag:yaml.org,2002:str'
    style = None
    if '\n' in data:
        style = '|'
    else:
        style = ''

    return self.represent_scalar(tag, data, style=style)

yaml.add_representer(str, represent_str)

with open('lng_tmpl/_base.yml', encoding='utf-8') as f:
    base = yaml.safe_load(f)

for file in os.listdir('lng'):
    try:
        assert file.endswith('.lng') and file.startswith("lang_")
    except AssertionError:
        continue
    base_name = file[5:-4]
    output = {
        "comments": [],
        "translations": {},
    }
    n_counter = 0
    with open(f'lng/{file}', encoding='utf-8') as f:
        for line in f.readlines():
            if line.startswith("#"):
                line = line.strip().strip("#").strip()
                print(line)
                output["comments"].append(line)
            else:
                base_string = base['gui_strings'][n_counter]
                line = line.strip()
                label = base_string['label']
                if not line or line == base_string['string']:
                    output["translations"][label] = [
                        "untranslated",
                        {"original": base_string['string'],},
                    ]
                else:
                    output["translations"][label] = line

                n_counter += 1
    output['comments'] = '\n'.join(output['comments']).strip()
    with open(f'lng_src/{base_name}.yml', 'w', encoding='utf-8') as f:
        yaml.dump(
            output,
            f,
            encoding='utf-8',
            allow_unicode=True,
            sort_keys=False,
            canonical=False,
            default_flow_style=False,
            default_style=''
        )
