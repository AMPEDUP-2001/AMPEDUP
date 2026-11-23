# AMPEDUP! 0.7.3 — Game language selection, Windows stability and Vulkan caching

Choose the game language before you play. This Windows release also fixes two resource leaks during course changes and lets Vulkan reuse pipelines from previous runs.

## Choose the game language before you play

Open **AMPEDUP!**, choose **Game language**, then press **Play**. In **RecompLauncher**, select **Game language** in the AMPEDUP! settings before launching. Both launchers use the same saved preference for that installation. The game starts in **English, Deutsch, Français, Italiano or Español** on subsequent launches. English is the default for a new profile.

Version 0.7.3 uses the selected language for the original European game text and the added **Graphics**, **Quit Game** and **Add Music Folder** entries, including the Graphics and Quit help text and the Windows folder-picker title.

| Language | Graphics | Quit Game | Add Music Folder |
| --- | --- | --- | --- |
| English | Graphics | Quit Game | Add Music Folder |
| Deutsch | Grafik | Beenden | Musikordner hinzufügen |
| Français | Graphismes | Quitter | Ajouter un dossier musical |
| Italiano | Grafica | Esci | Aggiungi cartella musicale |
| Español | Gráficos | Salir | Añadir carpeta de música |

## Windows fixes

- **Repeated course changes.** Windows now releases thread handles when game workers finish and cleans up events left behind by background Vulkan workers. After five visits alternating between Brighton and Snow Summit, the old crash reporter still held handles to 45 finished threads. The repaired reporter held zero after every return. A separate test found 19 newly created worker events still open on the old runtime and zero on the repaired runtime.
- **Course loading.** A thread-priority request during Brighton loading now reaches the correct Windows thread. When two requests arrive together, the game and Windows agree on the final priority.
- **Vulkan pipeline cache.** The game can now replace an existing cache file as it encounters new pipelines. It retains pending additions if a write fails or more pipelines arrive while it saves. In a Brighton and Snow Summit test, the old cache stayed at 30 KB. The repaired game saved 1.77 MB, and the NVIDIA driver loaded the full cache on the next launch.

## Snow Summit performance at 4×

Each drop was measured for 50 seconds per build on the same PC, with a 60 FPS cap, SMAA, 16× filtering and muted audio. The comparison used installed DEV and the repaired candidate, with the game in the foreground and the riders following closely matched paths.

| Drop | Average displayed FPS, DEV → candidate | Frames over 25 ms, DEV → candidate |
| --- | ---: | ---: |
| West Ridge | 58.24 → 58.90 | 34 → 14 |
| Miracle Mile | 59.63 → 59.65 | 4 → 6 |

West Ridge improved overall, with most of the reduction coming from one pair of runs. Miracle Mile missed the frame-pacing target. These measurements cover two Snow Summit drops on the test PC.

## Verification

- All 13 Windows regression tests passed. The candidate completed five consecutive course visits, played and skipped the original intro, and passed Graphics, FXAA persistence, restart cancellation and Quit checks at 4:3, 16:9 and 21:9.
- A fresh install verified all 610 files from the supported European XISO and built the Windows runtime with the localized menus. The launcher checks covered all five choices on new and existing profiles, saving a choice with **Play**, and closing the launcher without saving a change.
- The rebuilt game passed the same ten menu checks in each language at 4× and 16:9. These included opening Graphics, cancelling and confirming Quit, and opening and cancelling the Windows music-folder picker with its title in the selected language.

## Other work tested since 0.7.2

Separate development builds restored the glass, door and upper-gable flicker fixes at Molly’s Lodge in Brighton 1, reduced movie-specific checks during gameplay, corrected launcher text and tested recovery after an interrupted launcher upgrade. These additional visual and upgrade-recovery changes were tested separately from the runtime and language package described above.

## Deutsch · Français · Italiano · Español

<details>
<summary>Deutsch — Versionshinweise</summary>

**Version 0.7.3 für Windows.** Das Spiel gibt beim Streckenwechsel Ressourcen wieder frei. Nach fünf Besuchen in Brighton und Snow Summit hielt der alte Absturzmelder noch 45 Handles für bereits beendete Threads. Mit der Korrektur waren es null. Auch die 19 offenen Windows-Ereignisobjekte aus einem separaten Test der Vulkan-Hintergrundthreads wurden vollständig freigegeben. Beim Laden von Brighton wird die Priorität jetzt für den richtigen Windows-Thread gesetzt.

Vulkan kann eine vorhandene Pipeline-Cache-Datei aktualisieren. Im Test blieb die alte Datei bei 30 KB. Die Testversion speicherte 1,77 MB, die der NVIDIA-Treiber beim nächsten Start vollständig übernahm.

Bei 4× interner Auflösung sank die Zahl der Bilder mit mehr als 25 ms Anzeigezeit auf West Ridge von 34 auf 14. Die durchschnittliche Bildrate stieg von 58,24 auf 58,90 FPS. Auf Miracle Mile waren es vier gegenüber sechs langen Bildern bei 59,63 gegenüber 59,65 FPS. Pro Strecke und Version wurden 50 Sekunden gemessen. Die Messungen betreffen diese beiden Startpunkte auf dem Test-PC.

Alle 13 Windows-Regressionstests bestanden. Intro, Überspringen des Intros und die Menüs funktionierten in 4:3, 16:9 und 21:9. Bei einer Neuinstallation wurden alle 610 Dateien der europäischen Original-XISO geprüft und das Spiel mit den lokalisierten Menüs gebaut.

Im AMPEDUP!-Launcher unter **Game language** die Spielsprache wählen und **Play** drücken. In RecompLauncher steht dieselbe Auswahl in den AMPEDUP!-Einstellungen. Die Auswahl bleibt für spätere Starts gespeichert. In 0.7.3 folgen **Grafik**, **Beenden** und **Musikordner hinzufügen** der gewählten Sprache. Die Menüs wurden in allen fünf Sprachen der europäischen Ausgabe getestet. Weitere Tests betrafen flackernde Fenster, Tür und Giebel an Molly’s Lodge in Brighton 1, unnötige Videoabfragen beim Spielen, fehlerhafte Zeichen im Launcher und die Wiederherstellung nach einem unterbrochenen Launcher-Update.

</details>

<details>
<summary>Français — Notes de version</summary>

**Version 0.7.3 pour Windows.** Le jeu libère les ressources lors des changements de piste. Après cinq passages entre Brighton et Snow Summit, le gestionnaire de plantages conservait auparavant 45 références à des threads déjà terminés. Avec le correctif, il en restait zéro. Lors d’un essai distinct, les 19 objets événement Windows laissés ouverts par les threads Vulkan ont également tous été libérés. Le chargement de Brighton règle désormais la priorité du bon thread Windows.

Vulkan peut mettre à jour son cache de pipelines. Pendant le test, l’ancien fichier est resté à 30 Ko. La version corrigée a enregistré 1,77 Mo que le pilote NVIDIA a entièrement rechargés au démarrage suivant.

À une résolution interne de 4×, le nombre d’images affichées pendant plus de 25 ms sur West Ridge est passé de 34 à 14, et la moyenne de 58,24 à 58,90 images/s. Sur Miracle Mile, ce nombre est passé de quatre à six, pour des moyennes de 59,63 et 59,65 images/s. Chaque mesure couvre 50 secondes par piste et par version. Ces mesures concernent ces deux pistes sur le PC de test.

Les 13 tests de régression Windows ont réussi. La lecture de l’intro, la possibilité de la passer avec Start et les menus ont été vérifiés en 4:3, 16:9 et 21:9. Une nouvelle installation a vérifié les 610 fichiers de la XISO européenne d’origine et compilé le jeu avec les menus traduits.

Dans le lanceur AMPEDUP!, choisissez la langue du jeu dans **Game language**, puis cliquez sur **Play**. Dans RecompLauncher, le même réglage se trouve dans les paramètres d’AMPEDUP!. Le choix est conservé pour les prochains lancements. Dans la version 0.7.3, **Graphismes**, **Quitter** et **Ajouter un dossier musical** suivent la langue choisie. Les menus ont été testés dans les cinq langues de l’édition européenne. D’autres essais ont porté sur le scintillement des vitres, de la porte et du pignon de Molly’s Lodge à Brighton 1, les vérifications vidéo superflues pendant le jeu, le texte du lanceur et la reprise après une mise à jour interrompue.

</details>

<details>
<summary>Italiano — Note di versione</summary>

**Versione 0.7.3 per Windows.** Il gioco libera le risorse al cambio di pista. Dopo cinque passaggi tra Brighton e Snow Summit, il vecchio sistema di segnalazione degli arresti manteneva 45 riferimenti a thread già terminati. Con la correzione ne restavano zero. In una prova separata sono stati liberati anche tutti e 19 gli oggetti evento di Windows lasciati aperti dai thread Vulkan. Durante il caricamento di Brighton viene ora impostata la priorità del thread Windows corretto.

Vulkan può aggiornare la cache delle pipeline. Nella prova il vecchio file è rimasto a 30 KB. La versione corretta ha salvato 1,77 MB e il driver NVIDIA li ha ricaricati per intero all’avvio successivo.

Con risoluzione interna 4×, su West Ridge i fotogrammi rimasti sullo schermo per più di 25 ms sono scesi da 34 a 14, con una media salita da 58,24 a 58,90 FPS. Su Miracle Mile sono passati da quattro a sei, con medie di 59,63 e 59,65 FPS. Ogni misura copre 50 secondi per pista e per versione. Le misure si riferiscono a questi due percorsi sul PC di prova.

Sono stati superati tutti e 13 i test di regressione per Windows. L’intro, il salto dell’intro e i menu sono stati verificati in 4:3, 16:9 e 21:9. Una nuova installazione ha verificato tutti i 610 file della XISO europea originale e compilato il gioco con i menu tradotti.

Nel launcher di AMPEDUP!, scegli la lingua in **Game language**, quindi premi **Play**. In RecompLauncher trovi la stessa opzione nelle impostazioni di AMPEDUP!. La scelta viene salvata per gli avvii successivi. Nella versione 0.7.3, **Grafica**, **Esci** e **Aggiungi cartella musicale** seguono la lingua selezionata. I menu sono stati provati in tutte e cinque le lingue dell’edizione europea. Altre prove hanno riguardato lo sfarfallio di vetri, porta e timpano di Molly’s Lodge a Brighton 1, i controlli video superflui durante il gioco, il testo del launcher e il ripristino dopo un aggiornamento interrotto.

</details>

<details>
<summary>Español — Notas de la versión</summary>

**Versión 0.7.3 para Windows.** El juego libera los recursos al cambiar de pista. Tras cinco visitas alternas a Brighton y Snow Summit, el antiguo gestor de fallos conservaba 45 referencias a hilos que ya habían terminado. Con la corrección quedaron cero. En otra prueba también se liberaron los 19 objetos de evento de Windows que los hilos de Vulkan dejaban abiertos. Al cargar Brighton, el juego ahora ajusta la prioridad del hilo de Windows correcto.

Vulkan puede actualizar su caché de pipelines. En la prueba, el archivo anterior se quedó en 30 KB. La versión corregida guardó 1,77 MB y el controlador de NVIDIA los cargó por completo al iniciar el juego de nuevo.

Con la resolución interna a 4×, en West Ridge los fotogramas que permanecieron en pantalla más de 25 ms pasaron de 34 a 14, y la media subió de 58,24 a 58,90 FPS. En Miracle Mile pasaron de cuatro a seis, con medias de 59,63 y 59,65 FPS. Se midieron 50 segundos por pista y versión. Las mediciones corresponden a estas dos pistas en el PC de pruebas.

Se superaron las 13 pruebas de regresión de Windows. La reproducción de la intro, la opción de saltarla y los menús se comprobaron en 4:3, 16:9 y 21:9. Una instalación nueva verificó los 610 archivos de la XISO europea original y compiló el juego con los menús traducidos.

En el lanzador de AMPEDUP!, elige el idioma en **Game language** y pulsa **Play**. En RecompLauncher encontrarás la misma opción en los ajustes de AMPEDUP!. El idioma queda guardado para el siguiente inicio. En la versión 0.7.3, **Gráficos**, **Salir** y **Añadir carpeta de música** siguen el idioma elegido. Los menús se probaron en los cinco idiomas de la edición europea. Otras pruebas cubrieron el parpadeo de los cristales, la puerta y el frontón de Molly’s Lodge en Brighton 1, las comprobaciones de vídeo innecesarias durante el juego, el texto del lanzador y la recuperación tras una actualización interrumpida.

</details>
