# Compatibilité

## Voitures concernées

Une BMW avec une **unité EntryNav2** (appelée EntryEVO dans son système). Le sans-fil passe par le **Wi-Fi de
CarPlay** de l'unité ; sans lui, le [câble USB](USB.md) marche aussi. Écrans essayés en voiture : **1280x480** et
**800x480** (voir [plus bas](#autres-tailles-décran)).

Les NBT Evo, ID7 et MGU tournent sur d'autres systèmes : ce programme ne peut pas s'y exécuter. L'installation demande
aussi une connexion SSH à l'unité, avec son mot de passe root.

## Voitures essayées

La **voiture de référence** est celle sur laquelle le projet est développé et essayé. Les autres lignes viennent des
rapports de leurs propriétaires : elles disent ce qu'ils ont vu, avec la version qu'ils avaient alors.

| Voiture | Logiciel (`/etc/version`) | Écran | Liaison | Dernière version essayée | Ce qui a été vu |
|---|---|---|---|---|---|
| G20 de 2019 (référence) | `202111011321` | 1280x480 | sans fil, USB | 0.12.0-rc.12 | celles du [README](../README.fr.md#ce-que-ça-fait), limites dans la [feuille de route](../ROADMAP.md#limites-connues) |
| G20 de 2019 | `202207190806` | 1280x480 | sans fil | 0.12.0-rc.6 | marche ; lenteur au bout d'une minute avec un Galaxy Z Fold7, disparue en bloquant My BMW ([#44](https://github.com/Jiraiya43/EntryNav2-Android-Auto/issues/44)) |
| G21 320d de 2020 | `202402191339` | 1280x480 | sans fil | 0.12.0-rc.2 | marche, guidage au compteur avec Waze ; lenteur en sans-fil avec un Galaxy S26 Ultra ([#26](https://github.com/Jiraiya43/EntryNav2-Android-Auto/issues/26)) |
| X3 G01 de 2020 | `202003311616` | 1280x480 | sans fil, USB | 0.12.0-rc.6 | marche, double MEDIA ; flèches au compteur avec `kombi-single-maneuver` ; lenteur en sans-fil avec un Galaxy S24+ ([#17](https://github.com/Jiraiya43/EntryNav2-Android-Auto/issues/17)) |
| F30 330d LCI de 2018 | `202402191339` | 1280x480 | sans fil | 0.12.0-rc.6 | marche, ancien contrôleur ; touche MAP non reconnue sur ce contrôleur ([#43](https://github.com/Jiraiya43/EntryNav2-Android-Auto/issues/43)) |
| F45 225xe | non indiqué | 1280x480 | sans fil, USB | 0.12.0-rc.6 | marche ; lenteur en sans-fil, fluide par câble ; glissements tactiles à l'étude ([discussion #30](https://github.com/Jiraiya43/EntryNav2-Android-Auto/discussions/30)) |
| F20 118i de 2018 | `202402191339` | 800x480 | USB (unité sans Wi-Fi) | 0.12.0-rc.6 | marche ; pastille du volume pas vue ([#45](https://github.com/Jiraiya43/EntryNav2-Android-Auto/issues/45)) |
| i3 | `202012141616` | 800x480 | USB ; sans fil avec une clé Wi-Fi USB | 0.13.0-alpha.2 | marche, ancien contrôleur ([#19](https://github.com/Jiraiya43/EntryNav2-Android-Auto/issues/19)) |

Chaque nouvelle voiture s'ajoute ici, avec son rapport de compatibilité ([ticket « Compatibility report »](https://github.com/Jiraiya43/EntryNav2-Android-Auto/issues/new/choose)).

## Ce que le contrôle vérifie

Le contrôle est l'étape 2 de l'[installation](INSTALLATION.md#2-contrôler-la-voiture). Il ne modifie rien : les
programmes restent dans `/tmp`, en mémoire.

| Commande | Point | Si le point échoue |
|---|---|---|
| `md5sum -c MD5SUMS` | Archive arrivée intacte | Recopier l'archive |
| `df -k /` | 16 Mo libres sur le système | Bloquant |
| Chargeur de l'unité, en mode liste | **Toutes les bibliothèques et fonctions appelées existent sur cette voiture** | Bloquant |
| `aa-supervisor --compat` : `abi` | Interface de l'iDrive pour ses applis, de la même taille | Bloquant |
| `vmost` | Fonctions de la molette et des touches, chargées à l'exécution | Bloquant |
| `mic` | Fonctions du micro | Douteux |
| `video`, `order` | **Démarrage à blanc :** taille de l'écran, emplacement de l'image, superposition des couches | Bloquant |
| `screen` | Écran d'une autre taille que 1280x480 : accepté, mais jamais essayé en voiture | Douteux |
| `alps`, `uuid`, `spp` | **Démarrage à blanc :** bibliothèque Bluetooth de même structure, profil reconnu, état lisible | Bloquant ; Bluetooth occupé : douteux |
| `running` | Android Auto tourne déjà : l'écran et le Bluetooth ne sont pas contrôlés | Information |
| `hotspot`, `gps` | Wi-Fi de la voiture disponible, GPS de la voiture reconnu | Information |

Deux mécanismes font ce travail :

- **Le chargeur du système de la voiture, en mode liste,** résout toutes les fonctions sans rien exécuter. Il ne donne aucune
  fausse alerte sur le système de la voiture de référence, et il détecte une fonction ou une bibliothèque retirée exprès.
- **`aa-supervisor --compat`** fait les mêmes lectures que le vrai démarrage, sans rien enregistrer. Il ne change rien au
  Bluetooth, n'allume pas le Wi-Fi et n'affiche rien. Le Bluetooth est lu dans la mémoire du processus BMW, sans
  l'interrompre.

## Lire le résultat

| Résultat | Sens | Installation |
|---|---|---|
| Aucune ligne `@block`, logiciel `202111011321` | Même logiciel que la voiture de référence | Possible |
| Aucune ligne `@block`, autre logiciel ou lignes `@warn` | Même type d'unité, mais rien ne prouve que tout se comporte à l'identique | Possible, et réversible |
| Au moins une ligne `@block`, ou une fonction absente | Il manque quelque chose sur cette voiture | À ne pas faire |

« Possible, et réversible » sera le cas le plus fréquent sur une autre voiture : toutes les fonctions appelées existent et
le démarrage à blanc passe, mais le contrôle ne peut pas prouver qu'Android Auto marchera jusqu'au bout.

## Autres tailles d'écran

Le service lit la taille de l'écran sur la voiture et règle l'image d'Android Auto dessus, sans réglage à faire.
Android Auto ne sait dessiner que trois tailles : le service demande la plus petite qui contient l'écran, dit au
téléphone de garder son affichage dans la zone visible, et ne montre que cette zone.

| Écran | Image demandée au téléphone | Zone montrée | État |
|---|---|---|---|
| 1280x480 | 1280x720 | les 480 lignes du haut | essayé en voiture (référence) : rien ne change |
| 800x480 | 800x480 | tout | essayé en voiture sur une i3 ([#19](https://github.com/Jiraiya43/EntryNav2-Android-Auto/issues/19)) et une F20 ([#45](https://github.com/Jiraiya43/EntryNav2-Android-Auto/issues/45)), par câble |
| jusqu'à 1920x1080 | 1920x1080 | la zone de l'écran, en haut à gauche | testé sur PC, jamais essayé |

La ligne `@ok|video|` du contrôle donne la taille lue. Un écran plus grand que 1920x1080 est refusé. En 800x480,
l'image et le menu de l'appli iDrive ont été vus en voiture, pas encore la pastille du volume ({iss(45)}).

## Une autre version de la bibliothèque Bluetooth

Android Auto emprunte un emplacement de la pile Bluetooth BMW, puis le rend. La pile en crée six, de 0 à 5, et BMW n'en
règle que deux : le 0, celui des BMW Apps, où se connecte l'appli My BMW, et le 1, celui de CarPlay. Android Auto prend le
5, et laisse le 0 à My BMW. Sur une voiture où le 5 n'est pas démarré ou porte un autre identifiant, il prend le 0, comme
avant : My BMW ne trouve alors plus la voiture tant qu'Android Auto tourne. Le journal du superviseur dit lequel il a pris
(`Android Auto takes SPP 5` ou `takes SPP 0`).

Pour vérifier cet emplacement, Android Auto lit les tables de la pile en mémoire. Sur une autre voiture :

1. Les tables sont trouvées dans la bibliothèque de cette voiture, par leurs noms exportés.
2. Leurs trois tailles doivent être celles de la voiture de référence. Sinon, la voiture est refusée, et le rapport donne
   ces tailles.
3. Chaque lecture recoupe deux copies de chaque identifiant. Au moindre désaccord, rien n'est modifié.
4. Après l'emprunt, Android Auto relit la table pour y retrouver son propre identifiant. Sinon, il s'arrête et rend
   l'emplacement.

Cette acceptation par la structure n'a été essayée que sur une copie modifiée de la bibliothèque de référence. Dans le pire
cas, un redémarrage de l'iDrive rétablit le Bluetooth.

## Ce qui protège quand le contrôle ne peut pas savoir

| Si ça diffère | Ce qui se passe |
|---|---|
| Écran ou couche vidéo inattendus | La vidéo vérifie sa cible et refuse de démarrer |
| Format du GPS de la voiture | Retour au GPS du téléphone |
| Compteur injoignable | Pas de flèches, le reste marche |
| L'appli fait planter l'iDrive | L'appli se désactive au troisième essai |
| Wi-Fi indisponible | Le service s'arrête avec un message |

Ces protections sont testées sur PC, jamais sur une autre voiture que celle de référence.

## Flèches sur le compteur G01 / logiciel 146

Essai du 12 novembre 2026 : le compteur accepte le premier modèle de route du graphe capturé,
mais refuse le deuxième (méthode `0x000E`, modèle `0x27000001`, réponse `0x26`). Les dix-sept
requêtes précédentes réussissent ; ce n'est pas un refus systématique du client ni de la langue.

Le fichier `/var/local/data/AA/kombi-single-maneuver`, lu au démarrage du service, sélectionne
un graphe fermé pour le prochain changement de direction : un texte, une route, les modèles de
la manœuvre 0, sa liste et l'état de guidage. Il ne crée pas les routes et textes supplémentaires
du trajet enregistré. Le démarrage comporte 14 requêtes sans voies ; la prochaine manœuvre et
sa distance réutilisent les mêmes modèles et positions. Sans ce fichier, le graphe historique
reste sélectionné.

L'utilisateur a confirmé les flèches avec Google Maps **et Yandex Maps** sur son G01 EntryNav2
002.146.002. Les 14 requêtes et la suppression du guidage ont été acquittées par le compteur.
Un long trajet, les changements successifs de manœuvre et les voies n'ont pas encore été validés
en roulant. La signification OEM exacte de `0x26` reste inconnue. L'essai en voiture portait sur
la base 0.12.0-rc.2 ; le portage sur la branche actuelle est vérifié séparément sur PC.

Pour activer ce mode ou revenir au graphe historique : [maintenance](MAINTENANCE.md#graphe-limité-à-la-prochaine-manœuvre).

## Double appui sur MEDIA

Le retour à une session connectée utilise le lecteur vMOST déjà actif dans le service, avec les états MEDIA
CCS35up (E5.03/C47/C4F) et CCS (E5.01/C20). Il ne change pas les abonnements ni les requêtes MOST, n'ajoute pas de
processus et ne remplace aucune ressource BMW. L'appli de l'iDrive ouvre l'image par son chemin *Ouvrir*, après
vérification de la session, des appels, de la marche arrière et de l'arrêt. L'appui simple conserve son effet natif.

La détection, les doublons, les maintiens, les changements de protection et les demandes à usage unique sont testés
sur PC et sous QEMU avec les bibliothèques de la voiture. Le geste et son effet à l'écran n'ont pas encore été essayés
en voiture, sur aucun contrôleur. Usage : [maintenance](MAINTENANCE.md#revenir-à-android-auto-avec-media).
