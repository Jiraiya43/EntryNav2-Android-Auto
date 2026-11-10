# Journal des versions

Tous les changements notables de ce projet. Format inspiré de [Keep a Changelog](https://keepachangelog.com/fr/1.1.0/),
versions selon [SemVer](https://semver.org/lang/fr/).

Chaque version dit si elle a été **essayée en voiture** ou seulement **testée sur PC** (tests PC, et programmes ARM sous
QEMU avec les bibliothèques de la voiture).

## [Non publié]

### Corrigé

- Page vide de l'iDrive : ses délais (1, 3, 10 et 15 s) ne dépendent plus de l'heure de la voiture, que le GPS peut
  remettre à l'heure en roulant ([#2](https://github.com/Jiraiya43/entry-aad/issues/2)). Testé sur PC.
- Marche arrière : la page vide n'est plus redemandée par-dessus la caméra de recul quand celle-ci prend la main avant
  qu'Android Auto s'écarte ([#3](https://github.com/Jiraiya43/entry-aad/issues/3)). Testé sur PC.

### Modifié

- Origine des données venues d'ailleurs (certificat Android Auto, description du DHU de Google, images d'attente,
  relevés de la voiture) notée dans les fichiers et dans la licence. Les programmes sont inchangés.

## [0.10.0] - 2026-11-10 · voiture

Première version publique. Essayée sur une BMW G20 de 2019, logiciel iDrive `202111011321`.

### Ajouté

- **Android Auto sans fil** dans l'iDrive : le téléphone s'appaire en Bluetooth, puis rejoint le Wi-Fi de la voiture.
- **Appli iDrive** : ligne *Android Auto* du menu Media, avec Démarrer, Arrêter, démarrage automatique et aide. Android
  Auto lancé automatiquement reste à l'écran, molette sans effet sur l'iDrive.
- **Commandes d'origine** : molette, touches (NAV et MEDIA ouvrent les applis d'Android Auto), tactile, boutons du
  volant, bouton vocal vers l'Assistant, micro de la voiture.
- **GPS de la voiture** transmis au téléphone, jour et nuit, modes de conduite, caméra de recul, pastille du volume.
- **Guidage sur le compteur et la tête haute** : flèche, distance, rue, sortie de rond-point.
- **Contrôle de compatibilité** sans rien modifier (`evo-trial --compat`) et **procédure d'installation** par SSH
  ([docs/INSTALLATION.md](docs/INSTALLATION.md)), avec archive `entry-aad-X.Y.Z-armhf.tar` et empreintes.
- **Garde-fous** : l'appli se met en retrait si l'iDrive plante plusieurs fois après l'avoir chargée ; le Bluetooth est
  rendu à l'iDrive à l'arrêt.
- **Tests** : chaque octet envoyé au téléphone et au compteur est comparé à un relevé figé, sur PC et sous QEMU.

### Limites

- Essayée sur une seule voiture. La procédure d'installation a été répétée avec la BusyBox de la voiture sous QEMU, pas
  encore suivie sur une voiture.
- Voir les [limites connues](ROADMAP.md#limites-connues).

[Non publié]: https://github.com/Jiraiya43/entry-aad/compare/v0.10.0...HEAD
[0.10.0]: https://github.com/Jiraiya43/entry-aad/releases/tag/v0.10.0
