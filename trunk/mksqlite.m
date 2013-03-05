%MKSQLITE Eine MATLAB Schnittstelle zu SQLite
%  SQLite ist eine Embedded SQL Engine, welche ohne Server SQL Datenbanken
%  innerhalb von Dateien verwalten kann. MKSQLITE bietet die Schnittstelle
%  zu dieser SQL Datenbank.
%
% Genereller Aufruf:
%  dbid = mksqlite([dbid, ] SQLBefehl [, Argument])
%    Der Parameter dbid ist optional und wird nur dann ben�tigt, wenn mit
%    mehreren Datenbanken gleichzeitig gearbeitet werden soll. Wird dbid
%    weggelassen, so wird automatisch die Datenbank Nr. 1 verwendet.
%
% Funktionsaufrufe:
%  mksqlite('open', 'datenbankdatei')
% oder
%  dbid = mksqlite(0, 'open', 'datenbankdatei')
% �ffnet die Datenbankdatei mit dem Dateinamen "datenbankdatei". Wenn eine
% solche Datei nicht existiert wird sie angelegt.
% Wenn eine dbid angegeben wird und diese sich auf eine bereits ge�ffnete
% Datenbank bezieht, so wird diese vor Befehlsausf�hrung geschlossen. Bei
% Angabe der dbid 0 wird die n�chste freie dbid zur�ck geliefert.
%
%  mksqlite('close')
% oder
%  mksqlite(dbid, 'close')
% oder
%  mksqlite(0, 'close')
% Schliesst eine Datenbankdatei. Bei Angabe einer dbid wird diese Datenbank
% geschlossen. Bei Angabe der dbid 0 werden alle offenen Datenbanken
% geschlossen.
%
%  mksqlite('version mex')                 (1
% oder
%  version = mksqlite('version mex')       (2
% Gibt die Version von mksqlite in der Ausgabe (1), oder als String (2) zur�ck.
%
%
%  mksqlite('version sql')                 (1
% oder
%  version = mksqlite('version sql')       (2
% Gibt die Version der verwendeten SQLite Engine in der Ausgabe (1), 
% oder als String (2) zur�ck.
%
%  mksqlite('SQL-Befehl')
% oder
%  mksqlite(dbid, 'SQL-Befehl')
% F�hrt SQL-Befehl aus.
%
% Beispiel:
%  mksqlite('open', 'testdb.db3');
%  result = mksqlite('select * from testtable');
%  mksqlite('close');
% Liest alle Felder der Tabelle "testtable" in der Datenbank "testdb.db3"
% in die Variable "result" ein.
%
% Beispiel:
%  mksqlite('open', 'testdb.db3')
%  mksqlite('show tables')
%  mksqlite('close')
% Zeigt alle Tabellen in der Datenbank "testdb.db3" an.
%
% Parameter binding:
% Die SQL Syntax erlaubt die Verwendung von Parametern, die vorerst nur
% durch Platzhalter gekennzeichnet und durch nachtr�gliche Argumente
% mit Inhalten gef�llt werden.
% Erlaubte Platzhalter in SQLlite sind: ?, ?NNN, :NNN, $NAME, @NAME
% Ein Platzhalter kann nur f�r einen Wert (value) stehen, nicht f�r
% einen Befehl, Spaltennamen, Tabelle, usw.
%
% Beispiel:
%  mksqlite( 'insert vorname, nachname, ort into Adressbuch values (?,?,?)', ...
%            'Gunther', 'Meyer', 'M�nchen' );
%
% Statt einer Auflistung von Argumenten, darf auch ein CellArray �bergeben
% werden, dass die Argumente enth�lt.
% Werden weniger Argumente �bergeben als ben�tigt, werden die verbleibenden
% Parameter mit NULL belegt. Werden mehr Argumente �bergeben als
% ben�tigt, bricht die Funktion mit einer Fehlermeldung ab.
% Ein Argument darf ein realer numerischer Wert (Skalar oder Array)
% oder ein String sein. Nichtskalare Werte werden als Vektor vom Datentyp
% BLOB (uint8) verarbeitet.
%
% Beispiel:
%  data = rand(10,15);
%  mksqlite( 'insert data into MyTable values (?)', data );
%  query = mksqlite( 'select data from MyTable' );
%  data_sql = typecast( query(1).data, 'double' );
%  data_sql = reshape( data_sql, 10, 15 );
%
% BLOBs werden immer als Vektor aus uint8 Werten in der Datenbank abgelegt.
% Um wieder urspr�ngliche Datenformate (z.B. double) und Dimensionen 
% der Matrix zu erhalten muss explizit typecast() und reshape() aufgerufen werden.
% (Siehe hierzu auch das Beispiel "sqlite_test_bind.m")
% Wahlweise kann diese Information (Typisierung) im BLOB hinterlegt werden. 
% Die geschilderte Nachbearbeitung ist dann zwar nicht mehr n�tig, u.U. ist die 
% Datenbank jedoch nicht mehr kompatibel zu anderer Software!
% Die Typisierung kann mit folgendem Befehl aktiviert/deaktiviert werden:
%
%   mksqlite( 'typedBLOBs', 1 ); % Aktivieren
%   mksqlite( 'typedBLOBs', 0 ); % Deaktivieren
%
% (Siehe auch Beispiel "sqlite_test_bind_typed.m")
% Typisiert werden nur numerische Arrays und Vektoren. Strukturen, Cellarrays
% und komplexe Daten sind nicht zul�ssig und m�ssen vorher konvertiert werden.
%
% (c) 2008 by Martin Kortmann <mail@kortmann.de>
%

