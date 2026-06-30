# Tuning Fork

Tuning fork for tuning musical instruments and more

![tuning_fork](img/tuning_fork.gif)

## Tunings
 - Guitar
    - 6 strings: Standard, Drop D, DADGAD (Dsus4), Standard D, C#G#C#F#G#C# (C#sus4), Drop C, CGCFGC
    - 7 strings: Standard, Drop A, Standard A
 - Bass
    - 4 strings: Standard, Tenor, Drop D, Standard D, Drop C#, Drop C
    - 5 strings: Standard, Tenor, Drop A
 - Ukulele
    - 4 strings: Standard
 - Banjo
    - 5 strings: Standard
 - Cigar Box
    - 3 strings: Open G, Open D, Open A
    - 4 strings: Open G
 - Tuning Forks: Common A4 (440Hz), Sarti's A4 (436Hz), 1858 A4 (435Hz), 1750-1820 A4 (423.5Hz), Verdi's C4 (256Hz)
  - Scientific Pitch: C0 (16Hz), C1 (32Hz), C2 (64Hz), C3 (128Hz), C4 (256Hz), C5 (512Hz), C6 (1024Hz), C7 (2048Hz), C8 (4096Hz), C9 (8192Hz), C10 (16384Hz), C11 (32768Hz)

## Development

### Install ufbt
> Linux & macOS: `python3 -m pip install --upgrade ufbt`
> Windows: `py -m pip install --upgrade ufbt`

### Clone repo
```
git clone https://github.com/besya/flipperzero-tuning-fork.git
```
### Navigate to project folder
```
cd flipperzero-tuning-fork
```
### Prepare VSCode environment
```
ufbt vscode_dist
```
### Build app
```
ufbt
```
> This command creates dist/tuning_fork.fap

### Launch app
```
ufbt launch
```
> This command deploys app to Flipper Zero and launches an application

### Nova
For Nova there is a helpful script to setup dev env.

1. Install C-Dragon extension
2. Run `./nova.sh`
3. Update C-Dragon settings to use Custom clangd and set Language server Path to one produced by `./nova.sh` command
4. Restart Nova and enjoy

## Contibuting

1. Fork repo
1. Clone
1. Create branch
1. Make changes
1. Commit changes and push to branch
1. Open fork page and click Contribute
1. Create a pull-request from your branch to this repo's main branch

## Acknowledgements

Special thanks to:
- [@xMasterX](https://github.com/xMasterX) for distributing and support
- [@RogueMaster](https://github.com/RogueMaster) for distribution and support
- [@Willy-JL](https://github.com/@Willy-JL) for fixes
- [@panki27](https://github.com/@panki27) for inspiration
- The Flipper Zero community for support and feedback

## License

This project is licensed under the GNU GPL v3 - see the [LICENSE](LICENSE) file for details.
