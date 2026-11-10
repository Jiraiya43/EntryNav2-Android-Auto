# Journal des versions

Tous les changements notables de ce projet. Format inspiré de [Keep a Changelog](https://keepachangelog.com/fr/1.1.0/),
versions selon [SemVer](https://semver.org/lang/fr/).

Chaque version dit si elle a été **essayée en voiture** ou seulement **testée sur PC** (tests PC, et programmes ARM sous
QEMU avec les bibliothèques de la voiture).

## [Non publié]

## [0.11.0-rc.2] - 2026-11-11 · voiture

Version candidate, **essayée en voiture** sur une BMW G20 : Android Auto démarre en sans-fil comme par câble USB.
Restent à confirmer par câble : la tenue sur un long trajet, le débranchement en cours de route, et d'autres
téléphones et voitures. Les changements de la 0.11.0-rc.1 sont inclus.

### Ajouté

- Android Auto par câble USB, expérimental et facultatif : avec le fichier `/var/local/data/AA/usb`, le service
  attend un téléphone Android sur les prises USB de la voiture au lieu de préparer le Bluetooth et le Wi-Fi. Sans ce
  fichier, rien ne change : Android Auto reste sans fil. Le son reste sur le Bluetooth de la voiture.
  [Activation et limites](docs/USB.md). Idée, premiers essais en voiture et observations sur le service USB de BMW :
  [@wowDaiver](https://github.com/wowDaiver) ([#18](https://github.com/Jiraiya43/entry-aad/pull/18)). Essayé en voiture.
- Nouvelle écriture vers un appareil de la voiture : les requêtes du mode accessoire (AOA 51, 52, 53) et une
  réinitialisation USB, envoyées seulement à un téléphone Android branché, et seulement en mode USB. Aucune interface
  USB n'est prise à un pilote ou à un programme BMW.

## [0.11.0-rc.1] - 2026-11-10 · PC

Version candidate, **testée sur PC seulement** : à essayer en voiture avant la 0.11.0. Installée par-dessus la 0.10.0,
elle demande un redémarrage de l'unité.

### Corrigé

- Page vide de l'iDrive : ses délais (1, 3, 10 et 15 s) ne dépendent plus de l'heure de la voiture, que le GPS peut
  remettre à l'heure en roulant ([#2](https://github.com/Jiraiya43/entry-aad/issues/2)). Testé sur PC.
- Marche arrière : la page vide n'est plus redemandée par-dessus la caméra de recul quand celle-ci prend la main avant
  qu'Android Auto s'écarte ([#3](https://github.com/Jiraiya43/entry-aad/issues/3)). Testé sur PC.

### Modifié

- Sans fichier `vehicle`, le téléphone voit une BMW « iDrive » au lieu d'une « M340i », le modèle de la voiture d'essai
  ([#8](https://github.com/Jiraiya43/entry-aad/issues/8)). Le fichier `vehicle` donne le vrai modèle. Testé sur PC.
- Le superviseur `evo-trial` s'appelle maintenant `aa-supervisor` ([#7](https://github.com/Jiraiya43/entry-aad/issues/7)) :
  son programme est `/opt/aa/aa-supervisor`, et l'option `--trial-seconds` du service devient `--limit-seconds`.
  L'étape 3 de l'installation retire l'ancien programme. Testé sur PC.
- Le service annonce au téléphone la version du projet (fichier `VERSION`) comme version logicielle de l'unité, au lieu
  de la valeur fixe `1.3.33` ([#9](https://github.com/Jiraiya43/entry-aad/issues/9)). Testé sur PC.
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

[Non publié]: https://github.com/Jiraiya43/entry-aad/compare/v0.11.0-rc.2...HEAD
[0.11.0-rc.2]: https://github.com/Jiraiya43/entry-aad/releases/tag/v0.11.0-rc.2
[0.11.0-rc.1]: https://github.com/Jiraiya43/entry-aad/releases/tag/v0.11.0-rc.1
[0.10.0]: https://github.com/Jiraiya43/entry-aad/releases/tag/v0.10.0
