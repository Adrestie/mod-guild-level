# mod-guild-level
AzerothCore mod for guilds

# l'idée
Reprend le concept de niveau de guilde apparut à Cataclysm
avoir une interface dédiée ou modifier celle de https://github.com/Adrestie/mod-forever-ui si il est installé
les joueurs gagnent de l'xp de guilde quand ils font des activités en guilde (2 joueurs de la même guilde ou +)
chaque joueur rapporte de l'xp de guilde, ex : un groupe tue un monstre qui donne 10 xp, si dans le groupe 2 joueurs sont de la même guilde ça donne 20 xp. Si ils sont 5, ça donne 50 xp.
La guilde a une limite de 25 niveau.
au delà, la guilde passe sur un système de niveau de Parangon
les 25 premiers niveau donnent ces bonus : 
1. augmente le gain d'xp de 5%
2. réduit la perte de durabilité de 5%
3. augmente l'honneur gagné de 5%
4. réduit le temps de recharge de la pierre de foyer de 5 minutes
5. augmente parfois le nombre de matériaux trouvés lors d'une récolte de 1
6. réduit le prix en or des vendeurs de 5%
7. envoi 5% de l'or ramassé dans la banque de guilde
8. augmente la vitesse de déplacement en monture de 5%
9. augmente le gain de réputation de 5%
10. être ressuscité par un membre de la guilde rend toute la mana et les PV
11. augmente le gain d'xp de 10% supplémentaires
12. réduit la perte de durabilité de 15% supplémentaires
13. augmente l'honneur gagné de 10% supplémentaires
14. réduit le temps de recharge de la pierre de foyer de 10 minutes supplémentaires
15. augmente parfois le nombre de matériaux trouvés de 2 supplémentaires
16. réduit le prix en or des vendeurs de 5% supplémentaires
17. envoi 5% de l'or ramassé dans la banque de guilde supplémentaires
18. augmente la vitesse en monture de 5% supplémentaires
19. augmente le gain de réputation de 5% supplémentaires
20. augmente la durée des effets de consommables de 30 minutes
21. permet de téléporter les membres du raids sur le joueur (1h de CD)
22. permet de ressusciter les membres du raids (hors combat uniquement)
23. permet d'invoquer une banque de guilde temporaire (1h de CD)
24. permet d'invoquer un vendeur de composants - réparateur temporaire (1h de CD)
25. permet de se téléporter dans la maison de guilde

# Parangon
Le système de niveau parangon de guilde permet d'avoir une maison de guilde et d'octroyer des bonus aux joueurs de guilde qui jouent ensemble :
1. déblocage de la maison de guilde + boite aux lettres + apprentissage du sort pour se téléporter dans la maison de guilde
2. déblocage du vendeur de composant
3. déblocage du banquier
4. déblocage du réparateur
5. déblocage des maitres de classes + maitre des écuries
6. déblocage des maitres de professions (qui contiennent toutes les recettes des maitres de profession Vanilla, BC, Wotlk) + forge + enclume + autres Doodads nécéssaires à la création d'objets
7. déblocage de l'Hotel des ventes
8. déblocage des portails vers Orgrimmar / Stormwind, Thunderbluff / IronForge, Undercity / Darnassus, Exodar / Silvermoon, Grottes du temps, Shattrath, Dalaran
9. déblocage du mode "Customization"

# niveau 10 et au-delà
à partir du niveau 10, la guilde gagne des points de talents et débloque son arbre de talent.
se talents peuvent être attribué par le maitre de guilde ou les officiers et réinitialisé contre un montant d'or venant de la banque de guilde

l'arbre de talent sera définit comme avec ce module : https://github.com/Adrestie/mod-spheregrid

certains noeud seront stackable, ce seront des noeuds de statistiques (+10 endurance, +10 force, +5 score de coups critiques, etc.) et d'autres seront uniques.

# Mode Customization
quand la guilde atteint le niveau de parangon 9, ses joueurs peuvent personnaliser la maison de guilde avec des objets obtenus en donjons et raids.
il est possible de restreindre l'accès à cette fonctionnalité en fonction des rangs de guildes
les objets de customization ne peuvent apparaitre dans les tables de butin uniquement si la guilde a débloqué le mode de customization et que le groupe est composé de 4 membres de la même guilde pour un groupe, 8 membres de la même guilde pour un contenu pour un raid de 10 joueurs et 20 membres de la même guilde pour un raid de 25 joueurs
les objets de customization peuvent être lootés plusieurs fois et par n'importe qui de la guilde (dans un raid de 25 joueurs, seul les 20 joueurs de la guilde peuvent voir l'objet apparaitre dans leur fenêtre de butin).
si un joueur loot un objet, il peut l'utiliser pour l'ajouter aux objets de la guilde
une interface liste les objets que la guilde possède, ceux qu'elle ne possède pas avec un tooltip qui indique où le trouver
si le joueur a les droit de customization et qu'il se trouve dans la maison de guilde, il peut passer en mode "Customization" :
- son interface de base disparait pour laisser place à l'interface de customization
- il peut drag & drop un objet de l'interface d'objet de guilde pour le placer dans le mode
- il peut sélectionné un objet de customization et modifier sa position, sa rotation et sa taille
- il peut retirer un objet de customization en faisant un clic droit dessus
- le joueur possède un gizmo 3D pour l'aider dans sa manipulation
un objet ne peut être placé ou modifier que par un joueur à la fois
un objet ne peut pas être placé plus de fois que la guilde possède de ce nombre d'objet (il faut avoir looter 4 chaises pour en placer 4)
retirer un objet du monde doit rendre un stack de l'objet dans la guilde (j'ai 3 chaises placée et 1 dans l'inventaire de guilde, si j'en retire une le compteur de l'inventaire passe à 2)
