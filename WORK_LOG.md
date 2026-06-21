# Work Log

## TODO

NEXT:
- UNO Q dev env setup and update
- literature on re-using vape cells

## Mon 22 Jun 2026

Blog setup, based on prior art

~~## Thu 22 Apr 2026 - from Green Brain~~

added a Jekyll Github pages blog using the commands

```sh
mise use ruby@3.2.2
gem install jekyll bundler
jekyll new docs

cd docs

# downgrade jekyll to 3.9.6
# gem "jekyll", "~> 3.9.5" # to work with github-pages
bundle add github-pages webrick

# configure the _config.yml

# run
bundle exec jekyll serve --port 8888

# open
http://127.0.0.1:8888/vape-cell-EV/
```
