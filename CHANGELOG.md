# Journal des versions

Tous les changements notables de ce projet. Format inspiré de [Keep a Changelog](https://keepachangelog.com/fr/1.1.0/),
versions selon [SemVer](https://semver.org/lang/fr/).

Chaque version dit si elle a été **essayée en voiture** ou seulement **testée sur PC** (tests PC, et programmes ARM sous
QEMU avec les bibliothèques de la voiture).

## [Non publié]

### Ajouté

- SDK ARM facultatif : compilation des programmes sans copie du système de la voiture, avec des stubs utilisés
  uniquement pour l'édition de liens. Les résultats restent séparés des versions publiées ; les programmes ont encore
  besoin des bibliothèques de la voiture pour tourner. Testé sur PC.

## [0.13.0-rc.12] - 2026-11-27 · PC

Version candidate pour le sans-fil : l'économie d'énergie U-APSD retirée aussi au téléphone qui a rejoint le
Wi-Fi de la voiture tout seul, le fil qui lit le téléphone qui n'attend plus l'espacement des images, et une
ligne `[WifiLink]` sans chiffre trompeur, qui dit quand la puce Wi-Fi redémarre. S'installe par clé USB depuis la
0.13.0-rc.10. Testée sur PC et sous QEMU, **pas encore essayée en voiture**.

### Corrigé

- Sans-fil : un téléphone déjà sur le Wi-Fi de la voiture quand Android Auto démarre (réseau connu rejoint tout seul, téléphone revenu après une séance perdue) gardait l'économie d'énergie U-APSD toute la séance : le point d'accès ne reconstruisait sa balise sans U-APSD que si personne n'y était connecté. Sur une F52, c'était le cas des quatre séances sur quatre ([#203](https://github.com/Jiraiya43/EntryNav2-Android-Auto/issues/203)). La balise est maintenant reconstruite une fois par démarrage du point d'accès, même avec un appareil connecté : il est déconnecté une fois et rejoint aussitôt le Wi-Fi sans U-APSD, juste avant que le téléphone reçoive sa demande de démarrage. Un autre appareil sur ce Wi-Fi (ordinateur, iPhone en CarPlay) est déconnecté une fois aussi. U-APSD retiré a fait disparaître les à-coups sur une X1 F48 ([#138](https://github.com/Jiraiya43/EntryNav2-Android-Auto/issues/138)) ; il n'est pas la seule cause de lenteur sans fil. Testé sur PC.
- Journaux : la ligne `[WifiLink]` écrivait `0 retried` sur toutes les voitures, ce qui laissait croire que la puce Wi-Fi n'avait jamais à réémettre. Son pilote ne donne pas ce nombre (une seule tentative par trame, « no data about retries » dans son code) : il n'est plus écrit. La ligne dit à la place quand le pilote a redémarré le micrologiciel de la puce (`chip firmware restarted`), lu dans son état, sans rien demander à la puce, sur un fil à part : la boucle principale n'attend jamais le pilote plus de 200 ms. Les statistiques de la puce elle-même (réémissions, économie d'énergie du téléphone) ne sont pas lues : le pilote les demande à la puce, et sur une X1 F48 deux lectures à 5 s d'écart ont été suivies de deux redémarrages de son micrologiciel ([#256](https://github.com/Jiraiya43/EntryNav2-Android-Auto/pull/256), [#203](https://github.com/Jiraiya43/EntryNav2-Android-Auto/issues/203)). Testé sur PC.
- Sans-fil : les images qui arrivent par paires sont toujours espacées avant d'aller au décodeur, mais le fil qui lit
  le téléphone n'attend plus cet espacement. Ses accusés de réception et ses réponses aux pings du téléphone ne
  prennent donc plus jusqu'à 25 ms de retard par image retenue pendant les rafales d'images. En sans-fil, les images
  vont maintenant au décodeur par un seul fil, réservé à cela, dans leur ordre et chacune à son heure ; l'image
  retenue attend dans sa mémoire vidéo. Le fil qui lit le téléphone n'attend plus que lorsque les mémoires vidéo du
  décodeur sont toutes prises (rafale plus longue qu'elles), le temps qu'une se libère, comme avant. Le mode USB ne
  change pas. Testé sur PC.

## [0.13.0-rc.11] - 2026-11-26 · PC

Version candidate avant la 1.0. Elle reprend la relecture complète du code (audit du 25/11), la tête haute des
Série F apprise de la navigation BMW, le redémarrage propre après une mise à jour par clé USB, et les retours du
25-26/11 : plus de plantage du service à l'arrêt de la voiture, Android Auto qui revient après OPTION puis BACK,
plus de nom de rue ni d'adresse Wi-Fi dans les journaux. S'installe par clé USB depuis la 0.13.0-rc.10. Testée
sur PC et sous QEMU, **pas encore essayée en voiture**.

### Ajouté

- Traduction polonaise du menu Android Auto de l'iDrive, affichée quand l'iDrive est en polonais (au lieu de
  l'anglais). Comme les autres langues, les textes restent en ASCII, sans lettres accentuées (« laczenie » pour
  « łączenie ») ([#176](https://github.com/Jiraiya43/EntryNav2-Android-Auto/pull/176), essayée sur une F44 ; textes de la
  mise à jour par clé USB : [#185](https://github.com/Jiraiya43/EntryNav2-Android-Auto/pull/185)). Testé sur PC.
- Traduction italienne du menu Android Auto de l'iDrive ([#77](https://github.com/Jiraiya43/EntryNav2-Android-Auto/pull/77)). Testé sur PC.
- Après une mise à jour par clé USB, l'unité demande maintenant un redémarrage propre au processeur de l'unité, comme
  un outil de diagnostic BMW, au lieu du redémarrage qui laissait l'écran noir environ 3 minutes. S'il refuse, ne
  répond pas ou ne peut pas être joint, c'est le redémarrage d'avant. Le même à la main, voiture à l'arrêt :
  `aa-supervisor --reboot` ([maintenance](docs/MAINTENANCE.md#redémarrer-lunité-essai)). Testé sur PC, pas encore
  en voiture.

### Corrigé

- Sans-fil sur les écrans plus larges que 1280 pixels (10,25 pouces, 1440x540) : le service demandait au téléphone une
  image de 1920x1080, et le téléphone fermait la séance dès la mise en place de la vidéo, sur le Wi-Fi 2,4 GHz de la
  voiture. En sans-fil, il demande maintenant 1280x720 au plus : sur un écran plus large, le haut de l'image
  (1280x480) est étiré à tout l'écran, dans les mêmes proportions, et le toucher est ramené aux pixels de l'image. Par
  câble, rien ne change (1920x1080), ni sur les écrans de 1280 pixels ou moins
  ([#190](https://github.com/Jiraiya43/EntryNav2-Android-Auto/pull/190), essayé sans fil sur une F52 : image sur tout
  l'écran, toucher juste). Testé sur PC.
- Son : les demandes de focus audio du téléphone sont décodées comme des messages protobuf, même si leurs champs
  arrivent dans un autre ordre ou si leur valeur occupe plusieurs octets ([#121](https://github.com/Jiraiya43/EntryNav2-Android-Auto/pull/121)). Testé sur PC.
- Image : une demande de focus de la voiture arrivée avant la mise en place de la vidéo est désormais envoyée dès que
  la vidéo est prête, au lieu d'attendre un autre événement et de risquer d'écraser un état plus récent
  ([#123](https://github.com/Jiraiya43/EntryNav2-Android-Auto/pull/123)). Testé sur PC.
- Sans-fil : au démarrage, quand la pile Bluetooth de la voiture répondait « occupée » (statut 4) au renouvellement
  de l'annonce d'Android Auto, le service s'arrêtait et rendait le Bluetooth. Il attend maintenant le renouvellement
  suivant ([#97](https://github.com/Jiraiya43/EntryNav2-Android-Auto/pull/97), X1 F48, où la pile reste occupée près d'une minute au démarrage). Testé sur PC.
- Contrôle de compatibilité : un écran 1440x540 n'est plus signalé `@warn|screen|` comme jamais essayé en voiture. Il
  a marché sur une F52, par câble et sans fil ([#190](https://github.com/Jiraiya43/EntryNav2-Android-Auto/pull/190)) ; l'avertissement reste pour les autres tailles.
  Testé sur PC.
- Radio FM : en roulant, l'iDrive remonte lui-même sa page radio quand le tuner suit la station sur une autre
  fréquence, et Android Auto lui cédait la place comme après une touche (X1 F48, jusqu'à une vingtaine de fois par
  trajet). Sans touche laissée à l'iDrive dans les 6 secondes d'avant, l'iDrive est maintenant remis tout de suite
  sur la page vide d'Android Auto, au plus 4 fois en 10 s. Seulement pour cette page (écran 523, vu dans le journal
  de la voiture) : un autre écran que l'iDrive ouvre sans touche fait toujours céder la place à Android Auto
  ([#138](https://github.com/Jiraiya43/EntryNav2-Android-Auto/issues/138), d'après l'idée de [#139](https://github.com/Jiraiya43/EntryNav2-Android-Auto/pull/139)). Testé sur PC.
- Appli iDrive : quand la copie des journaux ou la vérification d'une mise à jour dépassait son délai (clé USB
  retirée ou défectueuse), l'appli attendait la fin du programme arrêté sur le fil de l'iDrive, qui pouvait geler
  tant que le noyau retenait ce programme. Les programmes lancés par l'appli sont maintenant récoltés à chaque tour
  de son état, sans jamais attendre ; le fil d'attente qui suivait le superviseur disparaît aussi. Testé sur PC.
- Appli iDrive : avant de remplacer des fonctions de son objet dans l'iDrive (début, erreur, fin), ou d'appeler
  celles qui lisent le codage de la voiture, l'appli vérifie qu'elles sont bien à l'endroit attendu. Sur une autre version du logiciel iDrive où elles auraient bougé, elle ne
  se crée pas (l'iDrive démarre sans elle, son journal dit pourquoi), ou ne lit pas le codage (touches de Série G,
  BMW), au lieu de risquer de le faire planter. Testé sur PC.
- Sans-fil : pendant une séance Android Auto, un lien Bluetooth qui s'ouvrait et se révélait être celui de l'appli
  My BMW (BMW Apps) pouvait recevoir la demande de démarrage sans fil d'Android Auto, surtout sur les voitures où
  Android Auto partage l'emplacement Bluetooth des BMW Apps. Ce lien est maintenant laissé à My BMW sans rien y
  écrire, comme hors séance. Testé sur PC.
- Arrêt d'Android Auto sans fil : quand le point d'accès Wi-Fi de la voiture ne répondait plus, l'arrêt pouvait
  attendre jusqu'à 15 s ses réglages, et le superviseur arrêtait alors le service de force au bout de 10 s, sans son
  arrêt propre. Plus aucun réglage n'est demandé au point d'accès une fois l'arrêt demandé : l'arrêt n'attend que celui
  en cours, 5 s au plus. Testé sur PC.
- Sans-fil : un téléphone qui rouvrait son lien Bluetooth très vite (0,1 s après l'avoir fermé, comme après une
  séance perdue) pendant que le service finissait de fermer l'ancien n'était plus entendu : ses messages étaient
  jetés, et Android Auto ne revenait qu'à sa connexion suivante, ou après un Arrêter/Démarrer. Le nouveau lien
  s'ouvre maintenant dès que l'ancien est fermé, avec ce que le téléphone a déjà envoyé. Testé sur PC.
- Contrôle de compatibilité : le Bluetooth était lu sur l'emplacement des BMW Apps (0) seulement, alors qu'Android
  Auto prend d'abord le 5. Avec My BMW connecté, le contrôle disait le Bluetooth occupé (`@warn|alps|`), et
  l'identifiant d'une autre appli sur le 0 pouvait le bloquer (`@block|uuid|`) sans rapport avec Android Auto. Il lit
  maintenant l'emplacement qu'un démarrage prendrait, choisi de la même façon, et le nomme (`@ok|alps|SPP 5`). Testé
  sur PC.
- Appli iDrive : la fenêtre vide que l'appli ouvre un instant pour ramener l'iDrive sur la page d'Android Auto
  pouvait rester ouverte quand l'iDrive y revenait de lui-même juste avant, et l'appli la prenait alors pour une
  fenêtre de l'iDrive : Android Auto se retirait derrière une fenêtre vide que rien ne refermait. Elle est refermée
  dès que la page vide ou le menu de l'appli revient, qu'Android Auto est ramené à l'écran, ou qu'un appel ou la
  caméra arrive, et n'est plus jamais prise pour une fenêtre de l'iDrive. Testé sur PC.
- Appli iDrive : quand la date de la voiture reculait (réglée par le GPS) après une touche, l'appli croyait cette
  touche toute récente aussi longtemps que la date restait en retard : Android Auto restait à l'écran après une
  touche qui devait le faire céder la place, ou cédait la place à la page radio que l'iDrive remonte seul. Une
  touche datée après l'heure actuelle ne compte plus comme récente. Après 24,8 jours de marche de l'unité sans
  redémarrage, son horloge en millisecondes débordait aussi : BACK vers le menu de l'appli ne ramenait plus la page
  d'Android Auto tout de suite. Testé sur PC.
- Appli iDrive : le garde-fou contre les plantages comptait chaque démarrage de l'iDrive arrêté en moins de 2 minutes,
  quel que soit son âge. Trois trajets très courts, même à plusieurs jours d'écart, éteignaient l'appli (et son menu)
  jusqu'à une intervention en SSH. Seuls les démarrages des 15 dernières minutes comptent maintenant, comme pour le
  démarrage automatique, et l'appli revient d'elle-même ensuite ; les fichiers des deux garde-fous ne peuvent plus
  rester à moitié écrits après une coupure de courant ([#4](https://github.com/Jiraiya43/EntryNav2-Android-Auto/issues/4)).
  Testé sur PC.
- Mise à jour par clé USB : quand elle s'arrêtait sans rien installer pendant qu'Android Auto s'arrêtait pour elle
  (un appel ou la marche arrière arrivés entre-temps, ou un arrêt trop long), Android Auto devait repartir, mais la
  relance était demandée trop tôt, refusée, et Android Auto restait arrêté sans rien dire. Il repart maintenant une
  fois complètement arrêté ; Démarrer ou Arrêter dans le menu entre-temps annule cette relance, et s'il n'est
  toujours pas arrêté 45 s plus tard, le journal le dit. Testé sur PC.
- Toucher : le service reconnaissait l'écran tactile au premier paquet reçu sur son port, quel qu'il soit. Un paquet
  venu d'ailleurs, par exemple d'un téléphone sur le Wi-Fi de la voiture, pouvait le lui faire ignorer pour toute la
  séance. L'écran est maintenant reconnu à sa première trame de toucher, arrivée par le réseau filaire de l'unité,
  jamais par le Wi-Fi. Testé sur PC.
- Image : le fil qui lit les messages du décodeur vidéo de la voiture ne regardait pas si leur file était encore
  ouverte. Si le décodeur la fermait, chaque lecture revenait aussitôt sans rien, et ce fil occupait un cœur du
  processeur à 100 % jusqu'à la fin de la séance. Il s'arrête maintenant ; sans ces messages, aucun tampon ne revient,
  et la séance se termine comme pour un décodeur bloqué. Testé sur PC.
- Image : une configuration vidéo du téléphone beaucoup plus grosse qu'une vraie (qui tient en quelques dizaines
  d'octets) était gardée et placée devant chaque image clé ; quand l'ensemble ne tenait plus dans le tampon du
  décodeur, toutes les images clés étaient perdues tant qu'elle restait. Une configuration de plus de 4 Kio est
  maintenant refusée, et la précédente reste. Testé sur PC.
- Flèches au compteur : sans compteur sur le réseau Ethernet (Série F, où le compteur passe par MOST), chaque essai
  d'envoi de flèche finissait par une annulation tentée pendant une minute, alors que rien n'avait pu partir :
  jusqu'à plusieurs minutes d'essais de connexion inutiles par séance. Une annulation n'est plus tentée que si
  quelque chose a vraiment été envoyé au compteur. Testé sur PC.
- Flèches au compteur et à la tête haute : une distance du téléphone qui n'était pas un nombre (« inf », « nan »),
  négative ou démesurée partait telle quelle, ou donnait au compteur une position calculée au hasard. Une telle
  distance compte maintenant comme absente : le compteur affiche la distance tirée des mètres, sinon « - ». Un texte
  court n'est plus affiché tel quel que s'il est fait de chiffres (« 0.3 », « 1,5 »), et un nom de rue qui n'est pas
  de l'UTF-8 valide n'arrive plus au compteur en caractères invalides. Testé sur PC.
- Flèches au compteur : les messages de navigation du téléphone étaient gardés entiers (jusqu'à 8 Mio chacun) et
  recopiés puis relus quatre fois par seconde, par le fil des flèches Ethernet comme par celui du compteur MOST.
  Seuls leurs premiers 32 Kio sont maintenant gardés (une étape en prend quelques centaines d'octets), au plus 16
  voies et 16 directions par voie sont lues, et les messages ne sont recopiés et relus que quand un nouveau est
  arrivé. Testé sur PC.
- Câble USB : un téléphone qui acceptait la demande de passer en mode accessoire sans jamais le faire était
  sollicité à nouveau toutes les 5 à 6 secondes, sans fin. Après 5 demandes restées sans suite, il est maintenant
  laissé tranquille jusqu'à ce qu'il soit débranché puis rebranché, et le journal le dit. Testé sur PC.
- Séance Android Auto : les messages en morceaux en cours d'assemblage gardent au plus 12 Mio pour tous les canaux
  ensemble (8 Mio par message, comme avant), et un message annoncé plus grand que 8 Mio est écarté dès son premier
  morceau. Un appareil qui ouvrait un message sur chacun des 256 canaux pouvait faire garder plus de 2 Gio au service
  et le faire planter. Testé sur PC.
- Séance Android Auto : un message trop court pour porter son numéro est écarté, au lieu d'être pris pour une image
  ou du son et accusé, même sur le canal de commande. Images et son ne sont plus attendus que sur les canaux vidéo et
  audio que la voiture annonce au téléphone ; un autre canal ne peut plus changer le numéro de flux qu'Android Auto
  attend dans les accusés de l'image. Testé sur PC.
- Séance Android Auto : les morceaux d'un même message doivent être tous chiffrés ou tous en clair, sinon le message
  est écarté ; une deuxième réponse de version du téléphone ne relance plus la négociation sécurisée (TLS) en cours
  de séance. Une fois la séance sécurisée, un message reçu en clair, autre qu'un ping, est encore traité comme avant,
  mais le journal du service le note (une fois par sorte de message), pour savoir si de vrais téléphones en envoient
  avant de les refuser. Testé sur PC.
- Séance Android Auto : quand la négociation sécurisée (TLS) avec le téléphone échoue, la séance se termine tout de
  suite, au lieu d'attendre 5 secondes de silence du téléphone. Un message de négociation reçu une fois la séance
  sécurisée est ignoré, au lieu de casser la séance au message suivant ; un message de négociation de plus de 64 Kio
  est refusé. Testé sur PC.
- Séance Android Auto : quand le téléphone ferme la liaison sécurisée (TLS), ou qu'un enregistrement abîmé suit un
  message lisible dans la même trame, la séance se termine à cette trame, au lieu de continuer jusqu'à la trame
  chiffrée suivante ou jusqu'à 5 secondes de silence. Testé sur PC.
- Séance Android Auto : une connexion qui n'a pas fini la négociation sécurisée (TLS) 10 secondes après son ouverture
  est fermée. Un appareil du Wi-Fi de la voiture qui envoyait de temps en temps une trame vide sans jamais négocier
  gardait la seule place de séance, et le téléphone ne pouvait plus se connecter. La négociation du téléphone
  précédent ne compte plus pour la connexion suivante. Testé sur PC.
- Séance Android Auto : la liaison sécurisée (TLS) ne propose jamais de compression, et une trame qui donnerait plus
  de 8 Mio une fois déchiffrée termine la séance. Aucun téléphone ne se sert de l'une ni ne fait l'autre : pour eux,
  rien ne change. Testé sur PC.
- Séance Android Auto : avant la fin de la négociation sécurisée (TLS), seuls les messages de la poignée de main
  passent en clair (version, TLS, ping, au revoir), 64 Kio au plus ; tout autre message est refusé et noté une fois.
  La liaison TLS n'accepte plus de renégociation demandée par le téléphone, ne propose plus de chiffrements anciens
  et libère sa mémoire à la fin de chaque séance. Testé sur PC.
- Séance Android Auto : la connexion du téléphone est lue par blocs (moins d'appels au système par image), le toucher
  et la molette restant prioritaires. Testé sur PC.
- Sans-fil : l'écoute du téléphone ne s'ouvre plus sur toutes les interfaces quand l'adresse du point d'accès est
  illisible, et ses connexions ne passent plus aux programmes lancés par le service. Testé sur PC.
- Sans-fil : l'échange Bluetooth avec le téléphone respecte ses délais même quand le téléphone envoie au compte-gouttes
  ou ne lit plus ; le gestionnaire de plantage du service est plus sûr. Testé sur PC.
- Appli iDrive : l'arrêt d'Android Auto vérifie qu'un numéro de processus est bien celui du superviseur ou du service
  avant de l'arrêter ; les fichiers de l'appli et du service ne suivent plus de lien symbolique. Testé sur PC.
- Appli iDrive : un service relancé entre deux sondages reçoit de nouveau l'état de la voiture (marche arrière, appel,
  aide au stationnement), une relance demandée juste après un lancement n'est plus perdue, et une erreur dans un rappel
  de l'iDrive ne remonte plus dans l'iDrive. Testé sur PC.
- Toucher : un pincement interrompu par une fenêtre de l'iDrive ou la marche arrière, ou par l'arrêt d'Android Auto,
  se termine bien côté téléphone (plus de doigt resté appuyé). Micro, pastille du volume, molette et GPS plus robustes.
  Testé sur PC.
- Sans-fil : un petit programme lancé par le service (réglage du point d'accès avant chaque connexion) recevait une
  copie de la connexion du téléphone et du port d'Android Auto. Si le service rouvrait son port pendant ce temps, il
  ne pouvait plus le prendre et le téléphone n'était plus accepté jusqu'au redémarrage, comme l'a vu un autre projet
  en voiture. Ces connexions ne passent plus aux programmes lancés. Testé sur PC.
- Appli iDrive : un autre projet a vu l'iDrive se bloquer puis l'unité redémarrer quand son appli lisait un service
  de l'iDrive depuis une tâche différée. Chez nous ces lectures (codage, réglage de la fenêtre du mode de conduite) ne
  se font qu'au démarrage de l'appli et à un changement de mode ; elles sont maintenant refusées dans une tâche
  différée, pour qu'un ajout futur ne puisse pas refaire cette erreur. Rien ne change en voiture. Testé sur PC.
- Flèches au compteur des Série F par MOST : quand le compteur ne répond pas aux champs connus (ceux du compteur 6WA),
  rien n'était dessiné ([#88](https://github.com/Jiraiya43/EntryNav2-Android-Auto/issues/88), MINI F56 : aucune des six
  tailles lues). Le service apprend maintenant les champs du compteur en écoutant la navigation BMW pendant un guidage,
  et les garde dans le fichier `kombi-most-layout` : un seul guidage BMW suffit, Android Auto les utilise ensuite. Sans
  rien envoyer au compteur pendant ce guidage, sans jamais garder ni écrire dans le journal le nom d'une rue. Le
  journal résume ce que la navigation fait de chaque champ, et dit une fois chaque réponse du compteur qui ne se
  rattache à rien (une lecture de taille refusée, par exemple). Testé sur PC, pas encore en voiture.
- Journaux : le journal du service n'écrit plus le nom des rues du guidage d'Android Auto (ni dans les lignes des
  flèches au compteur, ni dans les octets des messages de navigation du téléphone), seulement leur longueur. Ce
  journal est souvent publié dans les tickets, et une rue dit où se trouve le conducteur. Testé sur PC.
- Appli iDrive : après OPTION puis BACK (ou l'inclinaison à gauche), Android Auto revient de lui-même au lieu d'une
  page vide ([#137](https://github.com/Jiraiya43/EntryNav2-Android-Auto/issues/137), MINI F54 ; même logiciel que
  les BMW). L'iDrive ignorait la demande du menu de l'appli tant que son panneau d'options était ouvert, et rien ne
  ramenait l'image une fois le panneau fermé : il fallait un second BACK puis *Ouvrir Android Auto*. Un choix dans
  le panneau laisse Android Auto en retrait comme avant ; téléphone parti entre-temps : le menu de l'appli. Testé
  sur PC.
- Flèches au compteur des Série F (MOST) : une fois les champs du compteur trouvés, le service n'essaie plus le
  compteur Ethernet des Série G, qui faisait attendre quelques secondes à chaque essai ; et la ligne de journal de la
  première image ne fait plus croire à une panne quand la flèche s'affiche
  ([#79](https://github.com/Jiraiya43/EntryNav2-Android-Auto/issues/79)). Testé sur PC.
- Journaux : l'échange Bluetooth avec le téléphone n'écrit plus le nom ni l'adresse (BSSID) du Wi-Fi de la voiture
  dans le journal du service, seulement s'ils sont là : ces journaux sont publiés dans les tickets. Testé sur PC.
- Arrêt de la voiture : le service ne plante plus quand le service vMOST de la voiture s'arrête en premier
  (`free(): invalid pointer`, vu dans des journaux de la 0.13.0-rc.10). La bibliothèque vMOST de la voiture coupe
  elle-même sa connexion après une lecture ou un envoi raté, et une deuxième coupure la faisait planter : la molette,
  les flèches au compteur des Série F, la demande du point d'accès Wi-Fi et le redémarrage propre ne la coupent plus
  une deuxième fois. Testé sur PC.

## [0.13.0-rc.10] - 2026-11-24 · PC

Version candidate : la mise à jour par clé USB, sans PC (archive signée par le projet, tout vérifié avant
d'écrire, mise à jour coupée terminée à la suivante), et les flèches d'Android Auto sur le compteur des Série F par
MOST. Cette version s'installe encore par SSH (étapes 1 à 3 de l'installation) ; les suivantes pourront s'installer
par clé USB. Testée sur PC et sous QEMU, **pas encore essayée en voiture**.

### Ajouté

- **Mise à jour par clé USB**, sans PC ni SSH : l'archive d'une version publiée, copiée telle quelle à la racine d'une
  clé, puis *Mettre à jour par clé USB* dans le menu Android Auto. Un premier appui vérifie tout sans rien installer
  (signature du projet, empreinte de chaque fichier, programmes faits pour cette unité, bibliothèques et fonctions
  présentes sur la voiture) et dit ce qu'un second appui installe : version plus récente, plus ancienne, ou la même à
  réinstaller. Le second appui, jamais pendant un appel ni en marche arrière, arrête Android Auto, prend exactement
  l'archive vérifiée, écrit les nouveaux fichiers à côté des anciens, les relit, puis les met en place un par un par
  renommage ; l'unité redémarre d'elle-même. Refusée sans rien changer, l'installation relance Android Auto. Une
  coupure de courant pendant l'échange se termine à la mise à jour suivante, grâce à la liste gardée sur la partition
  de données. Rapport dans `EntryAA-update.txt` sur la clé et dans les journaux copiés
  ([installation](docs/INSTALLATION.md#par-clé-usb-sans-pc)). Les archives publiées portent désormais une liste
  `UPDATE` signée par la clé du projet ; une archive fabriquée soi-même (`tools/package_release.py --unsigned`)
  s'installe par SSH seulement. Cette version-ci s'installe encore par SSH, une fois ; les suivantes, par clé USB.
  Testé sur PC et sous QEMU avec la glibc et la libcrypto de la voiture, pas encore en voiture.
- Numéro de version dans le titre de la page *Aide* du menu Android Auto de l'iDrive (par exemple « Aide -
  0.13.0-rc.10 ») : une photo de la page suffit pour savoir quelle version tourne dans la voiture. Ses six lignes
  restent les mêmes. Testé sur PC.
- Le téléphone voit la série de la voiture (« BMW G20 », « MINI F56 ») au lieu de « iDrive » quand le fichier `vehicle`
  ne donne pas de modèle. L'appli de l'iDrive la lit dans le codage, au même endroit que CarPlay pour se présenter à un
  iPhone, sans jamais lire le reste du bloc, où se trouve le numéro de série de la voiture
  ([#15](https://github.com/Jiraiya43/EntryNav2-Android-Auto/issues/15)). La ligne `model=` du fichier l'emporte
  toujours. Testé sur PC.
- Flèches d'Android Auto sur le compteur des Série F, par MOST : flèche, distance, unité et rue, dans les champs de
  guidage que remplit la navigation BMW (bloc `HUD_Terminal`). Seulement sans compteur Ethernet (les Série G ne
  changent pas), jamais pendant un guidage BMW, rien de créé sur le compteur. Un compteur qui répond plusieurs secondes
  en retard après un démarrage à froid de l'iDrive est attendu (10 s par appel, huit appels d'avance), et la flèche
  suit le guidage quand il passe à la tête haute
  ([compatibilité](docs/COMPATIBILITE.md#flèches-sur-le-compteur-des-série-f),
  [#79](https://github.com/Jiraiya43/EntryNav2-Android-Auto/issues/79)). Testé sur PC seulement, d'après la description
  d'un compteur 6WA (F52).

### Corrigé

- Réception Android Auto : la limite de taille d'un message est vérifiée sans risque de débordement sur ARM, séparément
  pour chaque message. Un message trop grand est écarté et ses morceaux restants sont ignorés jusqu'à sa fin ; un
  message entier ne perturbe pas les fragments en cours
  ([#122](https://github.com/Jiraiya43/EntryNav2-Android-Auto/pull/122)). Testé sur PC.

## [0.13.0-rc.9] - 2026-11-22 · PC

Version candidate : des journaux sans PC (gardés après le sommeil de la voiture, copiés sur clé USB depuis le menu
de l'iDrive) et un sans-fil qui revient vite après une coupure (lien Bluetooth coupé par la pile de la voiture,
balise du point d'accès reconstruite une seule fois). Testée sur PC et sous QEMU, **pas encore essayée en voiture**.

### Ajouté

- Journaux sans PC ni SSH. Le menu Android Auto de l'iDrive a deux lignes de plus : *Garder les journaux*, qui active
  les journaux (le fichier `maintenance`), et *Copier les journaux sur clé USB*, qui les copie sur une clé USB
  branchée dans la voiture, dans un dossier `EntryAA-logs/<date>`, avec la version du logiciel de l'unité. La ligne
  dit ensuite si la copie a marché, s'il n'y a pas de clé ou pas de journal. Rien n'est effacé ni changé sur l'unité
  ([maintenance](docs/MAINTENANCE.md#journaux)). Testé sur PC.

### Corrigé

- Journal du service perdu au sommeil de la voiture : il était en mémoire vive (`/tmp`), et celui d'un trajet avait
  disparu avant qu'on puisse le récupérer
  ([#101](https://github.com/Jiraiya43/EntryNav2-Android-Auto/issues/101),
  [#88](https://github.com/Jiraiya43/EntryNav2-Android-Auto/issues/88)). Avec les journaux activés, il est maintenant
  sur la partition de données, comme celui de l'appli : 512 Ko au plus, plus les 512 Ko d'avant dans
  `service.old.log`, surveillé toutes les 5 s pour ne jamais remplir la partition. `/tmp/entry-aa-service.log` reste
  un lien vers lui. Si la partition refuse de l'ouvrir, il retourne en mémoire vive comme avant. Testé sur PC.
- Sans-fil : après une coupure, le téléphone mettait 3 à 7 s à revenir au lieu de 2 s avec la 0.13.0-rc.8
  ([discussion #30](https://github.com/Jiraiya43/EntryNav2-Android-Auto/discussions/30), F45). Pendant qu'il rejoint
  le Wi-Fi, le téléphone rouvre son lien Bluetooth toutes les 400 ms environ, et le service faisait reconstruire la
  balise du point d'accès à chaque fois (jusqu'à 6 fois en 4 s), au risque de le déconnecter en pleine connexion. La
  balise n'est plus reconstruite qu'une fois par démarrage du point d'accès ; le réglage, lui, est toujours refait.
  Testé sur PC.
- Sans-fil : la relance du téléphone après une coupure (0.13.0-rc.7) ne coupait le lien Bluetooth que de notre côté.
  Le téléphone ne voyait rien et continuait d'attendre : revenu en 70 s, ou pas du tout, sur la MINI F56 de
  [#88](https://github.com/Jiraiya43/EntryNav2-Android-Auto/issues/88). Le service demande maintenant à la pile
  Bluetooth de la voiture de couper ce lien, comme le fait Arrêter : le téléphone le voit se fermer, le rouvre et
  reçoit une nouvelle demande, comme après Arrêter puis Démarrer, où il revenait en 2 s. Testé sur PC.

## [0.13.0-rc.8] - 2026-11-22 · PC

Version candidate : un sans-fil plus solide (plus d'U-APSD proposé au téléphone, séance relancée après un envoi raté
ou un chiffrement cassé, Bluetooth rendu à l'arrêt), une image qui ne fige plus après des messages perdus ou un
décodeur bloqué, le côté du conducteur lu dans le codage de la voiture, plus d'écran vide après la marche arrière.
Testée sur PC et sous QEMU, **pas encore essayée en voiture**.

### Ajouté

- Côté du conducteur lu dans le codage de la voiture : l'appli iDrive demande à l'iDrive si la voiture est codée
  volant à gauche ou volant à droite, comme elle le fait déjà pour la marque et les touches, et le service l'annonce au
  téléphone. Une voiture à volant à droite n'a plus besoin d'une ligne `side=right` dans le fichier `vehicle` ; cette
  ligne reste possible et l'emporte sur le codage. Si le codage ne dit rien de sûr, conduite à gauche comme avant.
  Le journal de l'appli écrit `driver side: left` ou `right`, celui du service le côté reçu. Testé sur PC.

### Modifié

- Sans-fil : une image qui arrive moins de 25 ms après la précédente attend le reste de ce délai avant d'aller au
  décodeur. Par le Wi-Fi, des images peuvent arriver par paires, à quelques millisecondes d'écart : l'écran ne dessine
  que la dernière reçue à chaque rafraîchissement, la première de la paire n'est jamais affichée, et l'image saccade
  sans que rien ne soit perdu sur le réseau. L'image qui suit une image retenue n'attend jamais : le retard ajouté reste
  de 25 ms au plus et ne s'accumule pas, et les images bien espacées passent tout de suite. Le mode USB ne change pas.
  La ligne du journal de maintenance écrite toutes les 10 s compte les images arrivées trop près de la précédente,
  celles retenues et la plus longue attente, pour voir si nos voitures ont ce défaut. Testé sur PC.

### Corrigé

- Sans-fil : à l'arrêt, le Bluetooth pouvait ne pas être rendu quand le téléphone cherchait encore la voiture, et
  les Démarrer suivants étaient alors refusés en moins d'une seconde. Tant que l'enregistrement Android Auto reste
  sur son emplacement Bluetooth, le téléphone peut s'y reconnecter avant que le superviseur, qui regardait
  l'emplacement toutes les 100 ms, l'ait vu libre, et la pile Bluetooth de la voiture refuse de changer
  l'enregistrement d'un emplacement connecté. C'est l'explication la plus probable de
  [#88](https://github.com/Jiraiya43/EntryNav2-Android-Auto/issues/88) (MINI F56, Galaxy Z Fold : deux Démarrer
  refusés après Arrêter). Le superviseur regarde maintenant l'emplacement toutes les 10 ms, remet l'enregistrement
  d'origine dès qu'il est libre, et redemande la déconnexion quand le téléphone est revenu le premier (4 fois au plus,
  8 s au plus). Les contrôles d'avant restent, et un retour non confirmé garde toujours son journal. Testé sur PC.
- Sans-fil : quand le Wi-Fi calait plusieurs secondes pendant un envoi de la voiture, la séance continuait sur une
  connexion cassée. L'envoi abandonné au bout de 5 s pouvait laisser un message coupé en deux, et tout ce que la voiture
  envoyait ensuite devenait illisible pour le téléphone, sans que la séance s'arrête. Un envoi raté met désormais fin à
  la séance comme une coupure : rien de plus ne part, et le téléphone est relancé comme après une séance perdue.
  Testé sur PC.
- Sans-fil : un message chiffré abîmé en route laissait la séance continuer sur un chiffrement mort. En TLS, un
  enregistrement qui échoue à sa vérification est fatal : plus rien ne se déchiffre ensuite, les bons non plus, et la
  séance attendait 15 trames illisibles de plus avant de s'arrêter. Le premier message que la voiture envoyait ensuite
  (souvent l'accusé de cette image) partait en plus avec l'alerte d'OpenSSL à sa place. La séance s'arrête maintenant à
  cette trame, sans rien envoyer de plus, et le téléphone est relancé comme après une séance perdue. Un enregistrement
  coupé sur deux trames reste lu normalement, et 16 trames de suite qui n'apportent aucun enregistrement complet
  arrêtent toujours la séance. Testé sur PC.
- Image (ou consignes de guidage) figée pour le reste de la séance après quelques messages perdus. Le téléphone n'envoie
  que 10 messages d'avance sans accusé de la voiture ; un message que la voiture écartait (image trop courte, morceaux
  arrivés incomplets, morceau illisible) n'était jamais accusé et gardait une de ces places pour de bon, et au dixième
  le flux s'arrêtait. Chaque message écarté est désormais accusé une fois, et le reste d'un message perdu en route est
  ignoré sans bruit jusqu'à son dernier morceau. Testé sur PC.
- Image : si le décodeur vidéo de la voiture cessait de rendre ses tampons (après une erreur de décodage, son chien de
  garde ou un abandon), l'image d'Android Auto restait figée jusqu'à la fin de la séance, le téléphone continuant
  d'envoyer des images qui ne trouvaient plus de place. Quand aucun tampon n'est revenu depuis 3 s alors que l'image
  est à l'écran, le service le note une fois et met fin à la séance comme à une coupure : le téléphone revient sur un
  décodeur neuf (sans fil : son lien Bluetooth est fermé pour repartir ; USB : il est redemandé). Les images suivantes
  n'attendent plus 20 ms chacune un tampon qui ne viendra pas. Testé sur PC.
- Micro de l'Assistant : un micro qui ne donnait plus de son bloquait la lecture, et avec elle l'arrêt d'Android Auto
  (Arrêter ou fin du service). Un micro qui retombait en panne aussitôt réparé était aussi fermé puis rouvert sans
  pause, en boucle, tant que l'Assistant écoutait. Le micro est désormais lu sans blocage, par tranches de 100 ms ;
  2 s sans son comptent comme une panne, il est rouvert après 1 s, et après 3 pannes de suite sans aucun son il attend
  la demande suivante du téléphone. L'arrêt reste immédiat. Testé sur PC.
- GPS de la voiture : une seule vitesse de 70 m/s (252 km/h) ou plus suffit pour qu'Android Auto ignore la vitesse de
  toutes les positions suivantes, jusqu'à la fin de la séance. Le service acceptait jusqu'à 150 m/s, et la vitesse
  calculée entre deux positions pouvait dépasser 70 m/s quand une position arrivait en retard. Toute vitesse envoyée
  reste maintenant sous 70 m/s : une position plus rapide part sans sa vitesse, et une vitesse calculée trop grande
  n'est pas envoyée, ni la direction qui va avec. Testé sur PC.
- Description de la voiture : le téléphone recevait toujours celle du modèle de Google (le Desktop Head Unit), soit
  une conduite à gauche, l'essence plus l'électrique et une prise de recharge Tesla Supercharger. Une voiture à volant
  à droite était donc annoncée à gauche, et Android Auto disposait son écran pour le mauvais côté. Le fichier
  `vehicle` accepte maintenant `side=left` ou `side=right` et `fuel=petrol` ou `fuel=diesel` (sans eux : le côté codé
  dans la voiture, voir « Ajouté », et l'essence) ; le téléphone reçoit le côté du conducteur, un seul carburant et
  aucune prise de recharge ([installation](docs/INSTALLATION.md)). Testé sur PC.
- Toucher : quand l'image d'Android Auto s'effaçait (volume, fenêtre de l'iDrive, marche arrière, MENU) au moment où
  le doigt se levait, le téléphone pouvait garder ce doigt posé jusqu'au toucher suivant. Le relâché arrivé juste après
  le départ de l'image n'était pas envoyé, et le service oubliait ce doigt sans plus rien avoir à relâcher. Le relâché
  d'un doigt que le téléphone a vu se poser lui est désormais toujours envoyé ; rien d'autre ne part une fois l'image
  partie. Testé sur PC.
- Touches : COM appuyé juste avant le début d'un appel restait enfoncé sur le téléphone jusqu'au départ de l'image.
  Pendant un appel, COM est laissé à l'écran d'appel de l'iDrive, et cette règle retenait aussi le relâché d'un appui
  déjà envoyé à Android Auto. Le relâché suit désormais toujours son appui ; un COM appuyé pendant l'appel reste à
  l'iDrive, comme avant. Testé sur PC.
- Appli iDrive : Android Auto pouvait se retirer sans qu'aucune touche soit appuyée. Quand l'iDrive quittait la page
  vide le temps d'un relevé, y revenait, puis la quittait encore le temps d'un relevé moins de 3 s plus tard, l'appli
  prenait ces deux départs pour une touche de l'iDrive confirmée (MENU, MEDIA…). Le premier départ est désormais
  oublié dès que la page vide revient ; une vraie touche est toujours confirmée au relevé suivant. Testé sur PC.
- Appli iDrive : écran vide après la marche arrière
  ([#88](https://github.com/Jiraiya43/EntryNav2-Android-Auto/issues/88), MINI F56). L'aide au stationnement reste
  allumée un moment après la marche arrière, et Android Auto restait caché tant qu'elle l'était. Quand BACK fermait
  sa vue (ou qu'elle se fermait seule), l'iDrive revenait sur la page vide d'Android Auto sans son image, jusqu'à la
  fin de l'aide au stationnement ou un passage par le menu. Ce retour sur la page vide, hors marche arrière, compte
  maintenant comme un choix d'Android Auto, qui reste affiché jusqu'au prochain changement de l'aide au
  stationnement. Le journal du service ne dit plus « picture back » en sortant de la marche arrière quand l'aide au
  stationnement cache encore l'image. Testé sur PC.
- Sans-fil : image et son par à-coups avec certains téléphones
  ([#138](https://github.com/Jiraiya43/EntryNav2-Android-Auto/issues/138), X1 F48). Le Wi-Fi de la voiture propose
  U-APSD, une économie d'énergie du téléphone, et la puce Wi-Fi retenait alors ses trames plusieurs secondes : sur la
  X1 F48 de peppearci, son compteur d'attentes U-APSD dépassées montait à 1700 en 12 minutes, et les à-coups ont
  disparu sans U-APSD. Avant chaque échange Wi-Fi avec le téléphone, le service règle maintenant hostapd pour ne
  plus le proposer, puis lui fait reconstruire sa balise, d'après laquelle le téléphone demande U-APSD en se
  connectant. Cette reconstruction déconnecte les appareils présents : elle n'est faite que si aucun n'est sur le
  Wi-Fi de la voiture, un iPhone en CarPlay n'est donc jamais coupé. Le réglage reste en mémoire, aucun fichier de la
  voiture ne change. Le journal du service dit ce qui a été fait (ligne `car access point:`,
  [maintenance](docs/MAINTENANCE.md)). Testé sur PC.

## [0.13.0-rc.7] - 2026-11-20 · PC

Version candidate : après une coupure du sans-fil, Android Auto revient tout seul quand le téléphone garde son lien
Bluetooth, au lieu d'attendre Arrêter puis Démarrer. Testée sur PC et sous QEMU, **pas encore essayée en voiture**.

### Corrigé

- Sans-fil : après une coupure, Android Auto ne revenait pas tout seul quand le téléphone gardait son lien Bluetooth
  ouvert. Le service répondait à ce lien sans jamais redemander au téléphone de revenir, et il fallait faire Arrêter
  puis Démarrer ([#88](https://github.com/Jiraiya43/EntryNav2-Android-Auto/issues/88) : MINI F56, Galaxy Z Fold, de
  retour en 2 s après le redémarrage manuel). Une séance perdue (téléphone muet 5 s, connexion rompue) ferme désormais
  ce lien : le téléphone le rouvre et reçoit une nouvelle demande, comme après un redémarrage. Quitter Android Auto
  sur le téléphone n'est pas une coupure, et le mode USB ne change pas. Testé sur PC.

## [0.13.0-rc.6] - 2026-11-20 · PC

Version candidate pour les lenteurs du sans-fil : la connexion Android Auto passe en priorité Wi-Fi « vidéo », comme le
CarPlay de la voiture. Aussi : la caméra après une marche arrière, les messages Android Auto incomplets. Testée sur PC
et sous QEMU, **pas encore essayée en voiture**.

### Modifié

- Sans-fil : la connexion Android Auto passe en priorité Wi-Fi « vidéo », comme le CarPlay de la voiture pour son
  image (code d'Apple dans le CarPlay de BMW : `IP_TOS` 0x80, catégorie Wi-Fi vidéo). Ce que la voiture envoie au
  téléphone (accusés d'image, toucher, touches, accusés TCP) passe alors avant le trafic ordinaire ; jusqu'ici, rien
  n'était marqué. Essai contre les lenteurs du sans-fil
  ([discussion #30](https://github.com/Jiraiya43/EntryNav2-Android-Auto/discussions/30),
  [#101](https://github.com/Jiraiya43/EntryNav2-Android-Auto/issues/101),
  [#129](https://github.com/Jiraiya43/EntryNav2-Android-Auto/issues/129)) : l'effet reste à mesurer en voiture. Le
  mode USB ne change pas. Testé sur PC.
- Réception Android Auto : réutilise les tampons pendant chaque connexion, sans changer les limites du protocole.
  Les tampons de messages assemblés de plus de 1 Mio sont libérés après usage. Testé sur PC.

### Corrigé

- Marche arrière : la sortie de marche arrière arrive parfois avant la fermeture de la caméra, et Android Auto pouvait
  revenir trop tôt. L'appli attend maintenant que l'iDrive revienne sur une page de l'appli, ou que le conducteur
  choisisse Android Auto (Ouvrir, entrée), 15 s au plus
  ([#114](https://github.com/Jiraiya43/EntryNav2-Android-Auto/pull/114), adapté du travail d'AramayisAntonyan). Testé
  sur PC, pas encore essayé en voiture.
- Réception Android Auto : un morceau de message (MIDDLE ou LAST) arrivé sans son début est rejeté, au lieu de donner
  une image ou un accusé de trop ([#115](https://github.com/Jiraiya43/EntryNav2-Android-Auto/pull/115), adapté de
  Sq00ky). Testé sur PC.

## [0.13.0-rc.5] - 2026-11-19 · PC

Version candidate : pas d'écran noir au changement de mode de conduite quand l'iDrive n'en affiche pas la fenêtre,
contrôle de compatibilité juste pour les voitures en USB, lien BMW Apps dans le journal du sans-fil. Testée sur PC et
sous QEMU, **pas encore essayée en voiture**.

### Ajouté

- Journal du sans-fil : chaque ligne `[WifiLink]` dit si un téléphone tient le lien Bluetooth des BMW Apps (My BMW,
  AAIdrive, option *Apps* du téléphone dans la voiture), qui passe par la même puce que le Wi-Fi. Avec l'option *Apps*
  activée, Android Auto sans fil démarrait mal sur une F40
  ([#71](https://github.com/Jiraiya43/EntryNav2-Android-Auto/issues/71)) : les journaux diront si ce lien tourne pendant
  les lenteurs. Lecture seule, journaux activés seulement. Testé sur PC.

### Corrigé

- Contrôle de compatibilité : un point Bluetooth raté (`alps`, `uuid`, `spp`) sortait `@block` comme s'il empêchait
  toute installation, alors que seul le sans-fil s'en sert. Son détail le dit maintenant (`wireless only`), et avec le
  fichier `/var/local/data/AA/usb` il sort en `@info`. Une ligne `@info|usb|` montre le mode USB choisi
  ([#106](https://github.com/Jiraiya43/EntryNav2-Android-Auto/issues/106) : F20 de 2017 en USB, bibliothèque Bluetooth
  d'une autre construction). Testé sur PC.
- Changement de mode de conduite : Android Auto s'effaçait 3,5 s pour laisser voir la fenêtre du mode, même quand
  l'affichage du mode était désactivé dans l'iDrive : écran noir à chaque changement
  ([discussion #28](https://github.com/Jiraiya43/EntryNav2-Android-Auto/discussions/28)). L'appli lit maintenant ce
  réglage de l'iDrive, et Android Auto reste à l'écran quand il est coupé. Si le réglage ne peut pas être lu, rien ne
  change. Lu dans le système de la voiture de référence et testé sur PC, pas encore essayé en voiture.

## [0.13.0-rc.4] - 2026-11-18 · PC

Version candidate : pastille du volume pendant un appel, contrôle de compatibilité à jour pour les écrans 800x480.
Testée sur PC et sous QEMU, **pas encore essayée en voiture** ; hors appel, rien ne change à l'écran.

### Corrigé

- Journal du service : la ligne `[SDR] main VideoConfig` affichait toujours 0 pour la marge gauche ; elle donne
  maintenant celle envoyée au téléphone (40 sur une MINI). Le téléphone recevait déjà la bonne valeur.
- Contrôle de compatibilité : un écran 800x480 n'est plus signalé `@warn|screen|` comme jamais essayé en voiture. Il a
  marché sur une i3, une F20, une X1 F48 et une MINI F57 ; l'avertissement reste pour les autres tailles.
- Pastille du volume pendant un appel : elle ne s'affichait pas. Pendant un appel, la molette et les touches du volant
  règlent le volume de l'appel, que l'iDrive donne sur une autre échelle (de 25 à 100 sur une MINI F56), et le service
  l'ignorait. Il le montre désormais après une touche ou à un changement, jamais à la seule arrivée de l'appel
  ([#88](https://github.com/Jiraiya43/EntryNav2-Android-Auto/issues/88)). Testé sur PC à partir du journal de cette
  voiture, pas encore essayé en voiture.

## [0.13.0-rc.3] - 2026-11-16 · PC

Version candidate : Android Auto en retrait pendant l'aide au stationnement ouverte par son bouton. Testée sur PC et
sous QEMU, **pas encore essayée en voiture** ; rien ne change tant que l'aide au stationnement reste éteinte.

### Ajouté

- Aide au stationnement ouverte par son bouton, sans marche arrière : Android Auto restait par-dessus la caméra. L'appli
  iDrive suit `sensors.PDCStatus` : quand l'aide au stationnement s'allume, Android Auto se met en retrait comme en
  marche arrière et revient à la fin. Comme elle reste allumée un moment après la marche arrière, choisir Android Auto
  pendant ce temps (Ouvrir, bouton d'entrée, double MEDIA) le garde à l'écran jusqu'à son prochain changement
  ([#26](https://github.com/Jiraiya43/EntryNav2-Android-Auto/issues/26)).
- Journal de l'appli : l'écran affiché par l'iDrive (`hmi.graphicalContext`), à chaque changement, pour connaître les
  numéros des écrans caméra et panorama de chaque voiture.

## [0.13.0-rc.2] - 2026-11-16 · PC

Version candidate : deux corrections de la 0.13.0-rc.1, le toucher des MINI et Start refusé après un recul de l'heure.
Testée sur PC et sous QEMU, **pas encore essayée en voiture** ; une BMW reçoit la même description de voiture et le
même toucher qu'avant.

### Corrigé

- MINI : le toucher tombait 40 pixels à droite du doigt depuis la 0.13.0-rc.1. Les bords cachés par le rond sont des
  marges pour le téléphone, dont l'affichage commence alors 40 colonnes plus loin, et il lit le toucher dans ses propres
  pixels : chaque point lui est désormais donné dans ce repère ([#78](https://github.com/Jiraiya43/EntryNav2-Android-Auto/issues/78),
  [#88](https://github.com/Jiraiya43/EntryNav2-Android-Auto/issues/88)). Rien ne change sur une BMW.
- Start refusé (« pressed again within 3 s ») après un arrêt, tant que l'heure de l'unité, reculée de quelques heures
  après le démarrage, n'avait pas rattrapé celle du lancement précédent. Les délais de l'appli iDrive (anti-rebond de
  Start, arrêt du service, démarrage automatique 30 s après l'iDrive, remise à zéro des gardes) se mesurent désormais
  sur l'horloge monotone de l'unité, que l'heure ne touche pas ; avant, le démarrage automatique pouvait attendre aussi
  longtemps ([#86](https://github.com/Jiraiya43/EntryNav2-Android-Auto/issues/86)).

## [0.13.0-rc.1] - 2026-11-15 · PC

Version candidate : la touche MAP vers la navigation d'Android Auto, les MINI reconnues (marque et écran rond), le
volume et la sourdine comme l'iDrive, en plus des corrections des 0.12.1-rc.1 à rc.4. Testée sur PC et sous QEMU,
**pas encore essayée en voiture** ; une BMW reçoit la même description de voiture qu'avant.

### Ajouté

- La touche MAP ouvre la navigation d'Android Auto quand il est à l'écran, comme NAV, au lieu de rendre l'écran à la
  carte BMW ; c'est ce que fait l'iDrive avec CarPlay. Seules MENU et OPTION quittent Android Auto
  ([#1](https://github.com/Jiraiya43/EntryNav2-Android-Auto/issues/1)).
- MINI : la marque codée dans l'iDrive est lue au démarrage ; une MINI s'annonce « MINI » au téléphone et non plus
  « BMW ». Son écran est encadré par un rond qui en cache les bords : le téléphone garde ses boutons hors de 40 colonnes
  de chaque côté sur un écran de 1280 de large, 25 sur 800 ([#56](https://github.com/Jiraiya43/EntryNav2-Android-Auto/issues/56),
  [#64](https://github.com/Jiraiya43/EntryNav2-Android-Auto/issues/64), [#78](https://github.com/Jiraiya43/EntryNav2-Android-Auto/issues/78)).
- Sourdine sur la pastille du volume : l'appui sur le bouton du volume la bascule dans l'iDrive, un cran de volume la
  lève ; la pastille montre le haut-parleur barré.
- Image cassée après une image vidéo perdue : la session redemande une image complète au téléphone, pas dans les
  5 premières secondes de la vidéo et au plus une fois par 10 s.

### Corrigé

- Pastille du volume ouverte seule à la connexion, barre vide : la voiture envoie aussi le niveau d'une autre source
  (minimum au-dessus de 0), et le même niveau plusieurs fois. Seul le volume de la molette ouvre la pastille, sur un
  changement ou juste après une touche de volume, comme la barre de l'iDrive ([#56](https://github.com/Jiraiya43/EntryNav2-Android-Auto/issues/56)).
- Flux chiffré cassé : après 16 trames d'affilée qui ne se déchiffrent pas, la session se termine au lieu de garder
  une image figée jusqu'à la limite de silence.
- Journal `[WifiLink]` : plus de faux « signal 0 dBm » (niveau non mesuré par la puce) ni de débit vers le téléphone,
  que le pilote ne connaît pas (il donnait 1 Mbit/s partout).

## [0.12.1-rc.4] - 2026-11-14 · PC

Version candidate, pour faire essayer le double appui sur MEDIA après une autre touche sur l'ancien contrôleur (F20
du #55), en plus de la 0.12.1-rc.3. Testée sur PC et sous QEMU.

### Corrigé

- Double appui sur MEDIA sans effet sur l'ancien contrôleur après une autre touche (MENU par exemple) : une autre
  touche faisait oublier l'état de MEDIA, et l'ancien contrôleur, qui n'envoie qu'un message par appui, ne le
  redonnait qu'au premier appui suivant, perdu pour le geste. Une autre touche annule maintenant le geste sans faire
  oublier cet état, et chaque appui de l'ancien contrôleur vaut un relâché juste avant. Trouvé et analysé sur une F20
  ([#55](https://github.com/Jiraiya43/EntryNav2-Android-Auto/issues/55)). Testé sur PC et sous QEMU.

## [0.12.1-rc.3] - 2026-11-14 · PC

Version candidate, pour faire essayer la touche MEDIA de la F20 du #55 (contrôleur codé 0x5141), en plus de la
0.12.1-rc.2. Testée sur PC et sous QEMU ; rien ne change sur un contrôleur type G.

### Corrigé

- Touche MEDIA d'une F20 au petit contrôleur ZBE3 toujours sans effet en 0.12.1-rc.1 : cette voiture est codée avec
  la variante 0x5141, et l'iDrive traduit alors sa touche média en code 15, laissé sans touche Android Auto. Le code
  15 ne vient que de ce bouton : il est maintenant MEDIA, comme le code 5 des autres petits contrôleurs F (ouvre la
  musique d'Android Auto, sert au double appui). Signalé avec le codage relevé sur la voiture
  ([#55](https://github.com/Jiraiya43/EntryNav2-Android-Auto/issues/55)). Testé sur PC et sous QEMU.

## [0.12.1-rc.2] - 2026-11-14 · PC

Version candidate, pour faire essayer le glissement du doigt sur une F45 (lecture de l'état du doigt comme l'iDrive),
en plus de la 0.12.1-rc.1. Testée sur PC et sous QEMU ; rien ne change sur la G20 de référence. Installée par-dessus
une version plus ancienne que la 0.12.1-rc.1, elle demande un redémarrage de l'unité.

### Corrigé

- Glissements du doigt reçus comme de simples appuis sur une F45 : impossible de faire défiler une liste ou de
  déplacer la carte, Android Auto ouvrait l'élément touché ([#30](https://github.com/Jiraiya43/EntryNav2-Android-Auto/discussions/30)).
  Le service lisait l'état du doigt sur 4 bits ; l'iDrive le lit sur 2 bits (appui, mouvement, relâché) et range
  les 2 bits voisins à part. Un écran qui met ces bits voisins voyait ses mouvements écartés, et le doigt paraissait
  levé au bout d'environ 100 ms. Le service découpe maintenant l'octet comme l'iDrive. Rien ne change sur la G20 de
  référence, dont l'écran laisse ces bits à 0. Testé sur PC et sous QEMU ; pas encore essayé en voiture.

## [0.12.1-rc.1] - 2026-11-14 · PC

Version candidate, pour faire essayer les touches lues selon le codage de la voiture sur des contrôleurs de série F
(F20, F45), en particulier les petits contrôleurs à 5 touches. La page *Aide* en 6 lignes a été vue en voiture sur la
G20 de référence ; les touches selon le codage ne sont testées que sur PC et sous QEMU. Elle change l'appli de
l'iDrive : installée par-dessus une version précédente, elle demande un redémarrage de l'unité.

### Corrigé

- Touches de l'ancien contrôleur lues de travers sur les petits contrôleurs de série F : sur une F20 (ZBE3 à
  5 touches), MEDIA était lu comme COM et COM comme MAP, et le double appui sur MEDIA ne marchait pas
  ([#55](https://github.com/Jiraiya43/EntryNav2-Android-Auto/issues/55)). L'iDrive choisit la touche de chaque bouton
  d'après le codage de la voiture (contrôleur type G ou F) : l'appli de l'iDrive lit maintenant ce même codage au
  démarrage et l'écrit pour le service (`/tmp/entry-aad-controller`), qui traduit les boutons comme l'iDrive. La
  touche média d'un petit contrôleur F ouvre la musique d'Android Auto et sert au double appui ; la touche NAV d'un
  contrôleur F ouvre sa navigation. Sans ce fichier (appli plus ancienne), la table d'avant reste. Sur un contrôleur
  type G, rien ne change. Testé sur PC et sous QEMU ; pas encore essayé en voiture.

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

[Non publié]: https://github.com/Jiraiya43/EntryNav2-Android-Auto/compare/v0.13.0-rc.10...HEAD
[0.13.0-rc.10]: https://github.com/Jiraiya43/EntryNav2-Android-Auto/releases/tag/v0.13.0-rc.10
[0.13.0-rc.9]: https://github.com/Jiraiya43/EntryNav2-Android-Auto/releases/tag/v0.13.0-rc.9
[0.13.0-rc.8]: https://github.com/Jiraiya43/EntryNav2-Android-Auto/releases/tag/v0.13.0-rc.8
[0.13.0-rc.7]: https://github.com/Jiraiya43/EntryNav2-Android-Auto/releases/tag/v0.13.0-rc.7
[0.13.0-rc.6]: https://github.com/Jiraiya43/EntryNav2-Android-Auto/releases/tag/v0.13.0-rc.6
[0.13.0-rc.5]: https://github.com/Jiraiya43/EntryNav2-Android-Auto/releases/tag/v0.13.0-rc.5
[0.13.0-rc.4]: https://github.com/Jiraiya43/EntryNav2-Android-Auto/releases/tag/v0.13.0-rc.4
[0.13.0-rc.3]: https://github.com/Jiraiya43/EntryNav2-Android-Auto/releases/tag/v0.13.0-rc.3
[0.13.0-rc.2]: https://github.com/Jiraiya43/EntryNav2-Android-Auto/releases/tag/v0.13.0-rc.2
[0.13.0-rc.1]: https://github.com/Jiraiya43/EntryNav2-Android-Auto/releases/tag/v0.13.0-rc.1
[0.12.1-rc.4]: https://github.com/Jiraiya43/EntryNav2-Android-Auto/releases/tag/v0.12.1-rc.4
[0.12.1-rc.3]: https://github.com/Jiraiya43/EntryNav2-Android-Auto/releases/tag/v0.12.1-rc.3
[0.12.1-rc.2]: https://github.com/Jiraiya43/EntryNav2-Android-Auto/releases/tag/v0.12.1-rc.2
[0.12.1-rc.1]: https://github.com/Jiraiya43/EntryNav2-Android-Auto/releases/tag/v0.12.1-rc.1
[0.12.0]: https://github.com/Jiraiya43/EntryNav2-Android-Auto/releases/tag/v0.12.0
[0.12.0-rc.12]: https://github.com/Jiraiya43/EntryNav2-Android-Auto/releases/tag/v0.12.0-rc.12
[0.12.0-rc.6]: https://github.com/Jiraiya43/EntryNav2-Android-Auto/releases/tag/v0.12.0-rc.6
[0.12.0-rc.2]: https://github.com/Jiraiya43/EntryNav2-Android-Auto/releases/tag/v0.12.0-rc.2
[0.12.0-rc.1]: https://github.com/Jiraiya43/EntryNav2-Android-Auto/releases/tag/v0.12.0-rc.1
[0.11.0-rc.1]: https://github.com/Jiraiya43/EntryNav2-Android-Auto/releases/tag/v0.11.0-rc.1
[0.10.0]: https://github.com/Jiraiya43/EntryNav2-Android-Auto/releases/tag/v0.10.0
