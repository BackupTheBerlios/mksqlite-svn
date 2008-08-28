/*
 * MATLAB Schnittstelle zu SQLite
 */

#include <stdio.h>
#include <winsock.h>

#include <mex.h>
#include "sqlite3.h"

// Version
#define VERSION "1.1 dev"

// Revision aus SVN
#include "svn_revision.h"

// #define Debug(s)    mexPrintf("mksqlite: "),mexPrintf(s), mexPrintf("\n")
#define Debug(s)

// Wir brauchen eine C-Schnittstelle, also 'extern "C"' das ganze
extern "C" void mexFunction(int nlhs, mxArray*plhs[], int nrhs, const mxArray*prhs[]);

// Flag: Willkommensmeldung wurde ausgegeben
static bool FirstStart = false;
// Flag: NULL as NaN ausgeben
static bool NULLasNaN  = false;

static const double g_NaN = mxGetNaN();

#define MaxNumOfDbs 5
static sqlite3* g_dbs[MaxNumOfDbs] = { 0 };

// Sprache: Index in Meldungen
static int Language = -1;

#define MSG_HELLO               messages[Language][ 0]
#define MSG_INVALIDDBHANDLE     messages[Language][ 1]
#define MSG_IMPOSSIBLE          messages[Language][ 2]
#define MSG_USAGE               messages[Language][ 3]
#define MSG_INVALIDARG          messages[Language][ 4]
#define MSG_CLOSINGFILES        messages[Language][ 5]
#define MSG_CANTCOPYSTRING      messages[Language][ 6]
#define MSG_NOOPENARG           messages[Language][ 7]
#define MSG_NOFREESLOT          messages[Language][ 8]
#define MSG_CANTOPEN            messages[Language][ 9]
#define MSG_DBNOTOPEN           messages[Language][10]
#define MSG_INVQUERY            messages[Language][11]
#define MSG_CANTCREATEOUTPUT	messages[Language][12]
#define MSG_UNKNWNDBTYPE        messages[Language][13]

#define NUM_MSGS                                   14

// 0 = english
static const char* messages_0[] = 
{
	"mksqlite Version " VERSION " " SVNREV ", an interface from MATLAB to SQLite\n"
    "(c) 2008 by Martin Kortmann <email@kortmann.de>\n"
    "based on SQLite Version %s - http://www.sqlite.org\n\n",
    
    "invalid database handle\n",
	"function not possible",
	"Usage: %s([dbid,] command [, databasefile])\n",
	"no or wrong argument",
	"mksqlite: closing open databases.\n",
	"Can\'t copy string in getstring()",
    "Open without Databasename\n",
    "No free databasehandle available\n",
    "cannot open database\n%s, ",
	"database not open",
	"invalid query string (Semicolon?)",
	"cannot create output matrix",
	"unknown SQLITE data type"
};

// 1 = german
static const char* messages_1[] = 
{
	"mksqlite Version " VERSION " " SVNREV ", ein MATLAB Interface zu SQLite\n"
    "(c) 2008 by Martin Kortmann <email@kortmann.de>\n"
    "basierend auf SQLite Version %s - http://www.sqlite.org\n\n",
    
    "ung�ltiger Datenbankhandle\n",
    "Funktion nicht m�glich",
	"Verwendung: %s([dbid,] Befehl [, datenbankdatei])\n",
	"kein oder falsches Argument �bergeben",
	"mksqlite: Die noch ge�ffneten Datenbanken wurden geschlossen.\n",
    "getstring() kann keine neue zeichenkette erstellen",
    "Open Befehl ohne Datenbanknamen\n",
    "Kein freier Datenbankhandle verf�gbar\n",
	"Datenbank konnte nicht ge�ffnet werden\n%s, ",
	"Datenbank nicht ge�ffnet",
    "ung�ltiger query String (Semikolon?)",
    "Kann Ausgabematrix nicht erstellen",
    "unbek. SQLITE Datentyp"
};

static const char **messages[] = 
{
    messages_0,
    messages_1
};

/*
 * Eine simple String Klasse
 */

class SimpleString
{
public:

		 SimpleString ();
		 SimpleString (const char* src);
	 	 SimpleString (const SimpleString&);
virtual ~SimpleString ();

	operator const char* ()
		{
			return m_str;
		}

	SimpleString& operator = (const char*);
	SimpleString& operator = (const SimpleString&);

private:

	char* m_str;
	static const char* m_EmptyString;
};

const char * SimpleString::m_EmptyString = "";


SimpleString::SimpleString()
	:m_str (const_cast<char*> (m_EmptyString))
{
}

SimpleString::SimpleString(const char* src)
{
	if (! src || ! *src)
	{
		m_str = const_cast<char*> (m_EmptyString);
	}
	else
	{
		int     len = (int) strlen(src);
		m_str = new char [len +1];
		memcpy (m_str, src, len +1);
	}
}

SimpleString::SimpleString (const SimpleString& src)
{
	if (src.m_str == m_EmptyString)
	{
		m_str = const_cast<char*> (m_EmptyString);
	}
	else
	{
		int     len = (int) strlen(src.m_str);
		m_str = new char [len +1];
		memcpy (m_str, src.m_str, len +1);
	}
}

SimpleString& SimpleString::operator = (const char* src)
{
	if (m_str && m_str != m_EmptyString)
		delete [] m_str;

	if (! src || ! *src)
	{
		m_str = const_cast<char*> (m_EmptyString);
	}
	else
	{
		int     len = (int) strlen(src);
		m_str = new char [len +1];
		memcpy (m_str, src, len +1);
	}

	return *this;
}

SimpleString& SimpleString::operator = (const SimpleString& src)
{
	if (&src != this)
	{
		if (m_str && m_str != m_EmptyString)
			delete [] m_str;

		if (src.m_str == m_EmptyString)
		{
			m_str = const_cast<char*> (m_EmptyString);
		}
		else
		{
			int     len = (int) strlen(src.m_str);
			m_str = new char [len +1];
			memcpy (m_str, src.m_str, len +1);
		}
	}

	return *this;
}

SimpleString::~SimpleString()
{
	if (m_str && m_str != m_EmptyString)
		delete [] m_str;
}

/*
 * Ein einzelner Wert mit Typinformation
 */
class Value
{
public:
    int          m_Type;

    SimpleString m_StringValue;
    double       m_NumericValue;
    
virtual    ~Value () {} 
};

/*
 * mehrere Werte...
 */
class Values
{
public:
	int     m_Count;
    Value*	m_Values;
    
    Values* m_NextValues;
    
         Values(int n) : m_Count(n), m_NextValues(0)
            { m_Values = new Value[n]; }
            
virtual ~Values() 
            { delete [] m_Values; }
};



/*
 * Die DllMain wurd dazu verwendet, um zu erkennen ob die Library von 
 * MATLAB beendet wird. Dann wird eine evtl. bestehende Verbndung
 * ebenfalls beendet.
 */
BOOL APIENTRY DllMain(HANDLE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
		break;
	case DLL_THREAD_ATTACH:
		break;
	case DLL_THREAD_DETACH:
		break;
	case DLL_PROCESS_DETACH:
        {
            bool dbsClosed = false;
            for (int i = 0; i < MaxNumOfDbs; i++)
            {
                if (g_dbs[i])
                {
                    sqlite3_close(g_dbs[i]);
                    g_dbs[i] = 0;
                    dbsClosed = true;
                }
            }
            if (dbsClosed)
            {
                mexWarnMsgTxt (MSG_CLOSINGFILES);
            }
        }
		break;
	}
	return TRUE;
}

// Einen String von MATLAB holen
static char *getstring(const mxArray *a)
{
   int llen = mxGetM(a) * mxGetN(a) * sizeof(mxChar) + 1;
   char *c = (char *) mxCalloc(llen,sizeof(char));

   if (mxGetString(a,c,llen))
      mexErrMsgTxt(MSG_CANTCOPYSTRING);
   
   return c;
}

void mexFunction(int nlhs, mxArray*plhs[], int nrhs, const mxArray*prhs[])
{
    /*
     * Get the current Language
     */
    if (Language == -1)
    {
        switch(PRIMARYLANGID(GetUserDefaultLangID()))
        {
            case LANG_GERMAN:
                Language = 1;
                break;
                
            default:
                Language = 0;
        }
    }
    
	if (! FirstStart)
    {
    	FirstStart = true;

        mexPrintf (MSG_HELLO, sqlite3_libversion());
    }
    
    /*
     * Funktionsargumente �berpr�fen
     *
     * Wenn als erstes Argument eine Zahl �bergeben wurde, so wird diese
     * als Datenbank-ID verwendet. Ist das erste Argument ein String, wird
     * Datenbank-ID 1 angenommen.
     * Alle weiteren Argumente m�ssen Strings sein.
     */

    int db_id = 0;
    int FirstArg = 0;
    int NumArgs = nrhs;
    
    if (nrhs >= 1 && mxIsNumeric(prhs[0]))
    {
        db_id = (int) *mxGetPr(prhs[0]);
        if (db_id < 0 || db_id > MaxNumOfDbs)
        {
            mexPrintf(MSG_INVALIDDBHANDLE);
            mexErrMsgTxt(MSG_IMPOSSIBLE);
        }
        db_id --;
        FirstArg ++;
        NumArgs --;
    }
    
    // Alle Argumente m�ssen Strings sein
    bool isNotOK = false;
    int  i;
    
    for (i = FirstArg; i < nrhs; i++)
    {
        if (! mxIsChar(prhs[i]))
        {
            isNotOK = true;
            break;
        }
    }
    if (NumArgs < 1 || isNotOK)
    {
        mexPrintf(MSG_USAGE);
        mexErrMsgTxt(MSG_INVALIDARG);
    }
    
    char *command = getstring(prhs[FirstArg]);
    SimpleString query(command);
    mxFree(command);
    
    Debug(query);
 
    if (! strcmp(query, "open"))
    {
        if (NumArgs != 2)
        {
            mexPrintf(MSG_NOOPENARG, mexFunctionName());
            mexErrMsgTxt(MSG_INVALIDARG);
        }
        
        char* dbname = getstring(prhs[FirstArg +1]);

        // Wurde eine db-id angegeben, dann die entsprechende db schliessen
        if (db_id > 0 && g_dbs[db_id])
        {
            sqlite3_close(g_dbs[db_id]);
            g_dbs[db_id] = 0;
        }
        
        // bei db-id -1 neue id bestimmen
        if (db_id < 0)
        {
            for (int i = 0; i < MaxNumOfDbs; i++)
            {
                if (g_dbs[i] == 0)
                {
                    db_id = i;
                    break;
                }
            }
        }
        if (db_id < 0)
        {
            plhs[0] = mxCreateScalarDouble((double) 0);
            mexPrintf(MSG_NOFREESLOT);
            mexErrMsgTxt(MSG_IMPOSSIBLE);
        }
        
        int rc = sqlite3_open(dbname, &g_dbs[db_id]);
        
        if (rc)
        {
            sqlite3_close(g_dbs[db_id]);
            
            mexPrintf(MSG_CANTOPEN, sqlite3_errmsg(g_dbs[db_id]));

            g_dbs[db_id] = 0;
            plhs[0] = mxCreateScalarDouble((double) 0);
            
            mexErrMsgTxt(MSG_IMPOSSIBLE);
        }
        
        plhs[0] = mxCreateScalarDouble((double) db_id +1);
        mxFree(dbname);
    }
    else if (! strcmp(query, "close"))
    {
        if (db_id < 0)
        {
            for (int i = 0; i < MaxNumOfDbs; i++)
            {
                if (g_dbs[i])
                {
                    sqlite3_close(g_dbs[i]);
                    g_dbs[i] = 0;
                }
            }
        }
        else
        {
            if (! g_dbs[db_id])
            {
                mexErrMsgTxt(MSG_DBNOTOPEN);
            }
            else
            {
                sqlite3_close(g_dbs[db_id]);
                g_dbs[db_id] = 0;
            }
        }
    }
    else
    {
        if (db_id < 0)
        {
            mexPrintf(MSG_INVALIDDBHANDLE);
            mexErrMsgTxt(MSG_IMPOSSIBLE);
        }
        
        if (!g_dbs[db_id])
        {
            mexErrMsgTxt(MSG_DBNOTOPEN);
        }

        if (! strcmpi(query, "show tables"))
        {
            query = "SELECT name as tablename FROM sqlite_master "
                    "WHERE type IN ('table','view') AND name NOT LIKE 'sqlite_%' "
                    "UNION ALL "
                    "SELECT name as tablename FROM sqlite_temp_master "
                    "WHERE type IN ('table','view') "
                    "ORDER BY 1";
        }

        if (sqlite3_complete(query))
        {
            mexErrMsgTxt(MSG_INVQUERY);
        }
        
        sqlite3_stmt *st;
        
        if (sqlite3_prepare_v2(g_dbs[db_id], query, -1, &st, 0))
        {
            if (st)
                sqlite3_finalize(st);
            
            mexErrMsgTxt(sqlite3_errmsg(g_dbs[db_id]));
        }

        int ncol = sqlite3_column_count(st);
        if (ncol > 0)
        {
            char **fieldnames = new char *[ncol];   // Die Feldnamen
            Values* allrows = 0;                    // Die Records
            Values* lastrow = 0;
            int rowcount = 0;
            
            for(int i=0; i<ncol; i++)
            {
                const char *cname = sqlite3_column_name(st, i);
                
                fieldnames[i] = new char [strlen(cname) +1];
                strcpy (fieldnames[i], cname);
                // ung�ltige Zeichen gegen _ ersetzen
                char *mk_c = fieldnames[i];
                while (*mk_c)
                {
                	if ((*mk_c == ' ') || (*mk_c == '*') || (*mk_c == '?'))
                    	*mk_c = '_';
                    mk_c++;
                }
            }
            
            // Daten einsammeln
            for(;;)
            {
                int step_res = sqlite3_step(st);

                if (step_res != SQLITE_ROW)
                    break;
               
                Values* RecordValues = new Values(ncol);
                
                Value *v = RecordValues->m_Values;
                for (int j = 0; j < ncol; j++, v++)
                {
                     int fieldtype = sqlite3_column_type(st,j);

                     v->m_Type = fieldtype;
                        
                     switch (fieldtype)
                     {
                         case SQLITE_NULL:      v->m_NumericValue = g_NaN;                                   break;
                         case SQLITE_INTEGER:	v->m_NumericValue = (double) sqlite3_column_int(st, j);      break;
                         case SQLITE_FLOAT:     v->m_NumericValue = (double) sqlite3_column_double(st, j);	 break;
                         case SQLITE_TEXT:      v->m_StringValue  = (const char*)sqlite3_column_text(st, j); break;
                                
                         default:	mexErrMsgTxt(MSG_UNKNWNDBTYPE);
                     }
                }
                if (! lastrow)
                {
                    allrows = lastrow = RecordValues;
                }
                else
                {
                    lastrow->m_NextValues = RecordValues;
                    lastrow = lastrow->m_NextValues;
                }
                rowcount ++;
            }
            
            sqlite3_finalize(st);

            if (rowcount == 0 || ! allrows)
            {
                if (!( plhs[0] = mxCreateDoubleMatrix(0,0,mxREAL) ))
                    mexErrMsgTxt(MSG_CANTCREATEOUTPUT);
            }
            else
            {
                int ndims[2];
                
                ndims[0] = rowcount;
                ndims[1] = 1;
                
                if (!( plhs[0] = mxCreateStructArray (2, ndims, ncol, (const char**)fieldnames)))
                {
                    mexErrMsgTxt(MSG_CANTCREATEOUTPUT);
                }
                
                lastrow = allrows;
                int i = 0;
                while(lastrow)
                {
                    Value* recordvalue = lastrow->m_Values;
                    
                    for (int j = 0; j < ncol; j++, recordvalue++)
                    {
                        if (recordvalue -> m_Type == SQLITE_TEXT)
                        {
                            mxArray* c = mxCreateString(recordvalue->m_StringValue);
                            mxSetFieldByNumber(plhs[0], i, j, c);
                        }
                        else if (recordvalue -> m_Type == SQLITE_NULL && !NULLasNaN)
                        {
                            mxArray* out_double = mxCreateDoubleMatrix(0,0,mxREAL);
                            mxSetFieldByNumber(plhs[0], i, j, out_double);
                        }
                        else
                        {
                            mxArray* out_double = mxCreateDoubleScalar(recordvalue->m_NumericValue);
                            mxSetFieldByNumber(plhs[0], i, j, out_double);
                        }
                    }
                    allrows = lastrow;
                    lastrow = lastrow->m_NextValues;
                    delete allrows;
                    i++;
                }
            }
            for(int i=0; i<ncol; i++)
                delete [] fieldnames[i];
            delete fieldnames;
        }
        else
        {
            int res = sqlite3_step(st);
            sqlite3_finalize(st);

            if (res != SQLITE_DONE)
            {
                mexErrMsgTxt(sqlite3_errmsg(g_dbs[db_id]));
            }            
        }
    }
    
    Debug("fertig");
}

/*
 *
 * Formatierungsanweisungen f�r den Editor vim
 *
 * vim:ts=4:ai:sw=4
 */
