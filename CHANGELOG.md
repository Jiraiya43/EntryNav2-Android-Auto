# Journal des versions

Tous les changements notables de ce projet. Format inspiré de [Keep a Changelog](https://keepachangelog.com/fr/1.1.0/),
versions selon [SemVer](https://semver.org/lang/fr/).

Chaque version dit si elle a été **essayée en voiture** ou seulement **testée sur PC** (tests PC, et programmes ARM sous
QEMU avec les bibliothèques de la voiture).

## [Non publié]

### Corrigé

- Flèches absentes sur le compteur du G01 / EntryNav2 002.146.002 : le compteur refusait le deuxième modèle
  de route du trajet capturé avec `0x26`, puis le service effaçait le guidage. Créer
  `/var/local/data/AA/kombi-single-maneuver` et redémarrer Android Auto sélectionne un graphe limité à la
  prochaine manœuvre ; sans ce fichier, le graphe historique reste inchangé. Aucun nouvel appel réseau : un
  sous-ensemble des méthodes de guidage existantes est envoyé. Essayé en voiture le 12 novembre 2026 sur la
  base 0.12.0-rc.2 : flèches confirmées avec Google Maps et Yandex Maps, démarrage et nettoyage acquittés.
  Le portage actuel est vérifié sur PC ; long trajet, manœuvres successives et voies restent à valider en roulant.

## [0.12.0-rc.6] - 2026-11-12 · voiture

Version candidate, **essayée en voiture** sur une BMW G20, en sans-fil : l'iDrive accepte la nouvelle appli, et
Android Auto vient tout seul à l'écran et y reste. Ce démarrage-là, l'iDrive a accepté la première demande : le
passage par la fenêtre de l'appli quand il refuse n'a tourné que dans les tests sur PC, comme l'adaptation à
l'écran et l'ancienne molette, confirmées par ailleurs sur une BMW i3. Elle change l'appli de l'iDrive : installée
par-dessus une version précédente, elle demande un redémarrage de l'unité.

### Ajouté

- Écrans d'une autre taille que 1280x480 : le service lit la taille de l'écran sur la voiture et demande au
  téléphone l'image qui lui convient (800x480, 1280x720 ou 1920x1080), sans réglage. Le contrôle de compatibilité
  ne bloque plus sur la taille : il la donne (`@ok|video|800x480`) et prévient qu'elle n'a pas été essayée
  (`@warn|screen|`). Sur un écran 1280x480, ce qui est envoyé au téléphone est identique, octet pour octet.
  [Détails](docs/COMPATIBILITE.md#autres-tailles-décran) ([#19](https://github.com/Jiraiya43/entry-aad/issues/19)).
  Essayé en voiture sur une i3 à écran 800x480, par câble ; les autres tailles sont testées sur PC.
- Journaux : chaque connexion Bluetooth que la voiture ouvre, accepte ou perd dit maintenant si l'appareil est le
  téléphone d'Android Auto ou un autre (aucune adresse n'est écrite), et chaque coupure donne la raison du
  Bluetooth de la voiture (`reason=`). Le Bluetooth et le Wi-Fi de l'unité partagent la même puce : ces lignes
  montrent ce que fait le Bluetooth pendant qu'Android Auto passe par le Wi-Fi.
- Service : le lien Bluetooth d'Android Auto qui revient pendant une session est maintenant écouté. Sur cette
  unité le téléphone le rouvre toutes les 12 secondes environ ; le service n'y écrit toujours rien le premier, pour
  ne pas relancer la session en cours, mais il répond à un ping et note dans le journal ce que le téléphone envoie.
- Journaux : toutes les 10 secondes d'une session, sans fil ou par câble, le plus long silence du téléphone
  (`[Session] in 10 s: the phone's longest silence ... ms`). Un téléphone qui projette envoie quelque chose chaque
  seconde : plusieurs secondes de silence sont une liaison qui a calé, ce que les lignes `[WifiLink]` et
  Bluetooth voisines aident à expliquer.

### Corrigé

- Android Auto qui s'affichait tout seul à l'ouverture de la voiture, puis repartait au bout de 4 secondes : il
  fallait le rouvrir à la main. L'iDrive n'exécute aucune demande d'écran d'une appli tant qu'une de ses propres
  fenêtres est au-dessus, et voiture ouverte mais pas démarrée, cela peut durer jusqu'à l'appui d'une touche.
  Depuis la 0.9.4 l'appli retirait alors Android Auto. Maintenant le service le garde en retrait à la connexion,
  le temps que l'appli place l'iDrive sur sa page. Si l'iDrive refuse, l'appli ouvre sa propre fenêtre, vide, ce
  que l'iDrive permet à une appli pour passer devant, demande sa page dessous, la referme, et Android Auto
  s'affiche : 2 secondes en tout. Si même cela est refusé, Android Auto s'affiche quand même et y reste, et
  l'appli redemande sa page chaque seconde ; d'ici là la molette agit aussi sur l'écran de l'iDrive caché
  dessous. Change l'appli de l'iDrive : redémarrage de l'unité après l'installation.
- Android Auto qui refusait de démarrer pendant tout un trajet, avec la ligne `UDP 127.0.0.1:52790 already taken`
  dans le journal du service. L'appli de l'iDrive et le service se parlaient par un port réseau local, pris dans la
  plage que l'unité distribue au hasard à ses propres programmes : à certains allumages, un autre programme recevait
  ce port, et le service croyait à tort qu'un autre service tournait déjà. Ils se parlent maintenant par un socket
  Unix au nom abstrait, sans port : rien qu'un autre programme puisse recevoir par hasard. Défaut présent depuis la
  0.10.0, vu une fois sur la voiture de référence. Change l'appli de l'iDrive : redémarrage de l'unité après
  l'installation. Essayé en voiture.
- Molette muette sur une voiture à l'ancien contrôleur (BMW i3) : le service savait décoder ses messages (C10, C20)
  mais ne les demandait jamais, donc la voiture ne les lui donnait pas. Il les demande maintenant quand la molette
  ne répond pas dans le protocole de la G20 au bout de 2 secondes. Une voiture qui répond, comme la G20, ne reçoit
  rien de plus, et un seul protocole est gardé par voiture. Signalé par kedzior12
  ([#19](https://github.com/Jiraiya43/entry-aad/issues/19)), qui l'a essayé sur son i3 : rotation, appui et
  inclinaison arrivent dans Android Auto.

## [0.12.0-rc.2] - 2026-11-11 · PC

Version candidate, **testée sur PC seulement** : à essayer en voiture. Elle change l'appli de l'iDrive : installée
par-dessus la 0.12.0-rc.1, elle demande un redémarrage de l'unité.

### Ajouté

- Menu de l'iDrive : la case **Câble USB au lieu du sans-fil** choisit le mode, sans PC ni SSH. Elle pose ou retire
  le fichier `usb` ; si Android Auto tourne, il s'arrête puis redémarre dans le nouveau mode. Le mode n'est jamais
  déduit de ce qui est branché : un téléphone mis en charge par un passager ne change rien. Une ligne d'aide
  rappelle que le son reste en Bluetooth. [Détails](docs/USB.md). Testé sur PC.
- Journaux : la qualité du Wi-Fi avec le téléphone, une ligne `[WifiLink]` à la connexion puis toutes les 10 secondes
  (fréquence, signal, débits, trames réémises et perdues, temps d'aller-retour). Lecture seule, journaux activés
  seulement, rien en mode USB. [Lire ces lignes](docs/MAINTENANCE.md#qualité-du-wi-fi). Testé sur PC.

### Modifié

- Installation : le numéro de version ne s'écrit plus que deux fois (commande `scp` et ligne `V=`), au lieu d'être
  répété dans chaque commande ([#19](https://github.com/Jiraiya43/entry-aad/issues/19)).
- Feuille de route : le câble USB n'est plus « hors périmètre », ses essais restants y sont listés, et le premier
  rapport de compatibilité d'une autre voiture y est noté ([#17](https://github.com/Jiraiya43/entry-aad/issues/17)).

## [0.12.0-rc.1] - 2026-11-11 · voiture

Version candidate, **essayée en voiture** sur une BMW G20 : Android Auto démarre en sans-fil comme par câble USB.
Restent à confirmer par câble : la tenue sur un long trajet, le débranchement en cours de route, et d'autres
téléphones et voitures. Les changements de la 0.11.0-rc.1 sont inclus ; la 0.11.0 ne sortira pas à part.

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

[Non publié]: https://github.com/Jiraiya43/entry-aad/compare/v0.12.0-rc.6...HEAD
[0.12.0-rc.6]: https://github.com/Jiraiya43/entry-aad/releases/tag/v0.12.0-rc.6
[0.12.0-rc.2]: https://github.com/Jiraiya43/entry-aad/releases/tag/v0.12.0-rc.2
[0.12.0-rc.1]: https://github.com/Jiraiya43/entry-aad/releases/tag/v0.12.0-rc.1
[0.11.0-rc.1]: https://github.com/Jiraiya43/entry-aad/releases/tag/v0.11.0-rc.1
[0.10.0]: https://github.com/Jiraiya43/entry-aad/releases/tag/v0.10.0
