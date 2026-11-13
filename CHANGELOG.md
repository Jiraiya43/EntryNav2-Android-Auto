# Journal des versions

Tous les changements notables de ce projet. Format inspiré de [Keep a Changelog](https://keepachangelog.com/fr/1.1.0/),
versions selon [SemVer](https://semver.org/lang/fr/).

Chaque version dit si elle a été **essayée en voiture** ou seulement **testée sur PC** (tests PC, et programmes ARM sous
QEMU avec les bibliothèques de la voiture).

## [Non publié]

### Corrigé

- Page *Aide* : en voiture, elle n'affiche que 6 lignes d'environ 36 caractères et ne défile pas (ses lignes ne se
  sélectionnent pas). Le crédit, en 8e à 10e position, restait invisible, et plusieurs lignes étaient coupées. La
  page tient maintenant en 6 lignes courtes : le crédit en tête (« Gratuit : github.com/Jiraiya43 », le profil qui
  mène au dépôt), puis NAV, MEDIA, COM, MENU et BACK, et le téléphone appairé pour le son.

## [0.12.0] - 2026-11-13 · voiture

Première version stable de la série 0.12 : la 0.12.0-rc.12, essayée en voiture sur la G20 de référence, plus deux
lignes sur la page *Aide* de l'iDrive. Le code des trois programmes est celui de la rc.12 ; seul le numéro de version
annoncé au téléphone change. La page *Aide* modifiée n'a été vue que sur PC. Elle change l'appli de l'iDrive :
installée par-dessus une version précédente, elle demande un redémarrage de l'unité.

### Ajouté

- Page *Aide* du menu Android Auto de l'iDrive : deux lignes en bas disent que le logiciel est gratuit et ne se vend
  pas, avec l'adresse du dépôt (`github.com/Jiraiya43/EntryNav2-Android-Auto`). Elles restent visibles dans toute copie
  redistribuée de l'archive. Textes en anglais, français et allemand.

## [0.12.0-rc.12] - 2026-11-13 · voiture

Version candidate, **essayée en voiture** sur la G20 de référence (logiciel 202111011321), en sans-fil, avec un kit
d'essai identique : démarrage automatique à chaque mise en route, Android Auto sur l'emplacement Bluetooth 5,
double appui sur MEDIA, BACK, MAP et MENU. Les numéros 0.12.0-rc.7 à rc.11 n'ont servi qu'à des kits d'essai et
n'ont pas été publiés. L'archive s'appelle désormais `entrynav2-android-auto-<version>-armhf.tar`. Elle change l'appli
de l'iDrive : installée par-dessus une version précédente, elle demande un redémarrage de l'unité.

### Ajouté

- Retour à une session Android Auto connectée en arrière-plan par deux appuis courts sur MEDIA, espacés de
  500 ms au plus entre leurs relâchements. Le lecteur du contrôleur déjà présent dans le service détecte le
  geste ; l'appli de l'iDrive vérifie de nouveau la session, l'arrêt, les appels et la marche arrière avant
  d'ouvrir l'image par le chemin du bouton *Ouvrir*. L'appui simple garde son effet habituel ; à l'écran,
  MEDIA garde ses commandes Android Auto. Aucun nouveau processus, fournisseur MOST ou fichier BMW modifié.
  Proposé par [@wowDaiver](https://github.com/wowDaiver) ([#40](https://github.com/Jiraiya43/EntryNav2-Android-Auto/pull/40)).
  Essayé en voiture sur la G20 de référence (CCS35up, sans-fil) et sur un X3 G01 (par câble USB) ; pas encore
  sur l'ancien contrôleur.

### Modifié

- Le projet s'appelle désormais **EntryNav2 (EntryEvo) Android Auto**, et son dépôt
  `Jiraiya43/EntryNav2-Android-Auto`. L'ancienne adresse `Jiraiya43/entry-aad` redirige vers la nouvelle,
  téléchargements des versions publiées compris. Les noms sur la voiture ne changent pas : programmes, dossiers, appli
  de l'iDrive et réglages restent les mêmes, et une installation existante se met à jour comme avant.
- Bluetooth : Android Auto prend maintenant l'emplacement série 5 de la pile Bluetooth de la voiture, libre, au lieu
  de l'emplacement 0 des BMW Apps où se connecte l'appli My BMW. Tant qu'Android Auto tournait, My BMW ne trouvait
  plus la voiture, et le service BMW de cet emplacement traitait chaque lien comme le sien ; un utilisateur voyait la
  lenteur du sans-fil disparaître en bloquant My BMW
  ([#44](https://github.com/Jiraiya43/EntryNav2-Android-Auto/issues/44)). Si l'emplacement 5 n'est pas libre,
  Android Auto reste sur le 0 comme avant ; il est rendu à l'arrêt. Essayé en voiture sur la G20 de référence :
  Android Auto démarre et tient sur le 5, l'emplacement 0 reste à My BMW. L'effet sur la lenteur reste à confirmer
  chez ceux qui la voyaient.

### Corrigé

- Android Auto qui se retirait environ 1,5 seconde après un appui sur BACK, quand il avait été ouvert par NAV, MEDIA
  ou COM (par exemple BACK dans sa carte). L'iDrive ouvre alors aussi son propre écran en dessous, et BACK l'y
  ramène ; l'appli de l'iDrive prenait ce changement pour une touche qui quitte Android Auto. BACK et l'inclinaison à
  gauche sont maintenant traités comme NAV, MEDIA et COM : Android Auto garde l'écran et l'iDrive est remis sur la
  page vide. MENU, MAP et OPTION quittent toujours Android Auto, même juste après BACK. Vu dans les journaux d'un F30
  ([#43](https://github.com/Jiraiya43/EntryNav2-Android-Auto/issues/43)). Essayé en voiture sur la G20 de référence
  (BACK et inclinaison à gauche sous Android Auto, MAP et MENU qui en sortent) ; le cas exact du F30, BACK après NAV,
  reste à confirmer sur un F30.
- Session Android Auto coupée 5 secondes après MENU ou MAP quand aucune musique ne joue : certains téléphones
  n'envoient plus rien tant que l'iDrive a l'écran, et le service les croyait partis. Après 2 secondes sans
  nouvelles, l'unité envoie maintenant elle-même un ping au téléphone, puis un par seconde. Un téléphone qui répond
  garde sa session ; un téléphone qui ne répond pas la perd toujours au bout de 5 secondes, comme avant. Trouvé par
  [@wowDaiver](https://github.com/wowDaiver) sur un X3 G01 par câble USB
  ([#40](https://github.com/Jiraiya43/EntryNav2-Android-Auto/pull/40)). Essayé en voiture sur la G20 de référence,
  sans changement visible ; son téléphone envoie ses propres pings, donc le cas du téléphone muet n'a été vérifié
  que sur PC et sous QEMU.
- Flèches absentes sur le compteur du G01 / EntryNav2 002.146.002 : le compteur refusait le deuxième modèle
  de route du trajet capturé avec `0x26`, puis le service effaçait le guidage. Créer
  `/var/local/data/AA/kombi-single-maneuver` et redémarrer Android Auto sélectionne un graphe limité à la
  prochaine manœuvre ; sans ce fichier, le graphe historique reste inchangé. Aucun nouvel appel réseau : un
  sous-ensemble des méthodes de guidage existantes est envoyé. Essayé en voiture le 12 novembre 2026 sur la
  base 0.12.0-rc.2 : flèches confirmées avec Google Maps et Yandex Maps, démarrage et nettoyage acquittés.
  Le portage actuel est vérifié sur PC ; long trajet, manœuvres successives et voies restent à valider en roulant.
- Molette qui agissait encore sur l'iDrive sous Android Auto, quand celui-ci revenait par-dessus un écran de l'iDrive
  (double appui sur MEDIA, démarrage automatique) : l'iDrive refusait alors la page vide et Android Auto restait à
  l'écran sans elle. Après le premier refus, l'appli ouvre sa propre fenêtre vide et demande la page vide en dessous,
  comme quand l'iDrive refuse son menu au démarrage. Essayé en voiture sur la G20 de référence : double MEDIA sur
  l'écran Média et démarrage automatique, la page vide est obtenue et la molette ne commande plus qu'Android Auto.

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
  [Détails](docs/COMPATIBILITE.md#autres-tailles-décran) ([#19](https://github.com/Jiraiya43/EntryNav2-Android-Auto/issues/19)).
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
  ([#19](https://github.com/Jiraiya43/EntryNav2-Android-Auto/issues/19)), qui l'a essayé sur son i3 : rotation, appui et
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
  répété dans chaque commande ([#19](https://github.com/Jiraiya43/EntryNav2-Android-Auto/issues/19)).
- Feuille de route : le câble USB n'est plus « hors périmètre », ses essais restants y sont listés, et le premier
  rapport de compatibilité d'une autre voiture y est noté ([#17](https://github.com/Jiraiya43/EntryNav2-Android-Auto/issues/17)).

## [0.12.0-rc.1] - 2026-11-11 · voiture

Version candidate, **essayée en voiture** sur une BMW G20 : Android Auto démarre en sans-fil comme par câble USB.
Restent à confirmer par câble : la tenue sur un long trajet, le débranchement en cours de route, et d'autres
téléphones et voitures. Les changements de la 0.11.0-rc.1 sont inclus ; la 0.11.0 ne sortira pas à part.

### Ajouté

- Android Auto par câble USB, expérimental et facultatif : avec le fichier `/var/local/data/AA/usb`, le service
  attend un téléphone Android sur les prises USB de la voiture au lieu de préparer le Bluetooth et le Wi-Fi. Sans ce
  fichier, rien ne change : Android Auto reste sans fil. Le son reste sur le Bluetooth de la voiture.
  [Activation et limites](docs/USB.md). Idée, premiers essais en voiture et observations sur le service USB de BMW :
  [@wowDaiver](https://github.com/wowDaiver) ([#18](https://github.com/Jiraiya43/EntryNav2-Android-Auto/pull/18)). Essayé en voiture.
- Nouvelle écriture vers un appareil de la voiture : les requêtes du mode accessoire (AOA 51, 52, 53) et une
  réinitialisation USB, envoyées seulement à un téléphone Android branché, et seulement en mode USB. Aucune interface
  USB n'est prise à un pilote ou à un programme BMW.

## [0.11.0-rc.1] - 2026-11-10 · PC

Version candidate, **testée sur PC seulement** : à essayer en voiture avant la 0.11.0. Installée par-dessus la 0.10.0,
elle demande un redémarrage de l'unité.

### Corrigé

- Page vide de l'iDrive : ses délais (1, 3, 10 et 15 s) ne dépendent plus de l'heure de la voiture, que le GPS peut
  remettre à l'heure en roulant ([#2](https://github.com/Jiraiya43/EntryNav2-Android-Auto/issues/2)). Testé sur PC.
- Marche arrière : la page vide n'est plus redemandée par-dessus la caméra de recul quand celle-ci prend la main avant
  qu'Android Auto s'écarte ([#3](https://github.com/Jiraiya43/EntryNav2-Android-Auto/issues/3)). Testé sur PC.

### Modifié

- Sans fichier `vehicle`, le téléphone voit une BMW « iDrive » au lieu d'une « M340i », le modèle de la voiture d'essai
  ([#8](https://github.com/Jiraiya43/EntryNav2-Android-Auto/issues/8)). Le fichier `vehicle` donne le vrai modèle. Testé sur PC.
- Le superviseur `evo-trial` s'appelle maintenant `aa-supervisor` ([#7](https://github.com/Jiraiya43/EntryNav2-Android-Auto/issues/7)) :
  son programme est `/opt/aa/aa-supervisor`, et l'option `--trial-seconds` du service devient `--limit-seconds`.
  L'étape 3 de l'installation retire l'ancien programme. Testé sur PC.
- Le service annonce au téléphone la version du projet (fichier `VERSION`) comme version logicielle de l'unité, au lieu
  de la valeur fixe `1.3.33` ([#9](https://github.com/Jiraiya43/EntryNav2-Android-Auto/issues/9)). Testé sur PC.
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

[Non publié]: https://github.com/Jiraiya43/EntryNav2-Android-Auto/compare/v0.12.0...HEAD
[0.12.0]: https://github.com/Jiraiya43/EntryNav2-Android-Auto/releases/tag/v0.12.0
[0.12.0-rc.12]: https://github.com/Jiraiya43/EntryNav2-Android-Auto/releases/tag/v0.12.0-rc.12
[0.12.0-rc.6]: https://github.com/Jiraiya43/EntryNav2-Android-Auto/releases/tag/v0.12.0-rc.6
[0.12.0-rc.2]: https://github.com/Jiraiya43/EntryNav2-Android-Auto/releases/tag/v0.12.0-rc.2
[0.12.0-rc.1]: https://github.com/Jiraiya43/EntryNav2-Android-Auto/releases/tag/v0.12.0-rc.1
[0.11.0-rc.1]: https://github.com/Jiraiya43/EntryNav2-Android-Auto/releases/tag/v0.11.0-rc.1
[0.10.0]: https://github.com/Jiraiya43/EntryNav2-Android-Auto/releases/tag/v0.10.0
