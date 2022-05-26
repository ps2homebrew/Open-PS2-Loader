# Translation guide

## For translators

All translation files moved to the separate repository: <https://github.com/ps2homebrew/Open-PS2-Loader-lang>\\
Provide your submissions there.\\
Languages were moved there for reducing commit stress for the main repository.\\
Check if your `.yml` file has untranslated strings. Example:

```yml
  MENU:
  - untranslated
  - original: Menu
```

You should change this into:

```yml
  MENU: Menu_on_your_language
```

If you want to test your changes:

- run `make download_lng`
- make your changes int `lng_src/*.yml`
- run `make languages`
- test generated file from `lng/lang_*.lng`.

## For developers

If you add the language string into the code,\\
propose your changes into `lng_tmpl/_base.yml` with the necessary comments.\\
For example, if you add the string `_STR_NETWORK_STARTUP_ERROR`,\\
then you should update the base file like:

```yml
- comment: Generic network error message.
  label: NETWORK_STARTUP_ERROR
  string: '%d: Network startup error.'
```

After running `make languages` propose your changes into [the language repo](https://github.com/ps2homebrew/Open-PS2-Loader-lang).\\
Folder `lng_src` will contain updated files.

It is not recommended to rename or remove already existing strings.\\
In such a case, you will need to edit all language yml files manually.
