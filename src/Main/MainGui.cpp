/***************************************************************************
 *   (c) J�rgen Riegel (juergen.riegel@web.de) 2008                        *   
 *                                                                         *
 *   This file is part of the FreeCAD CAx development system.              *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU Library General Public License (LGPL)   *
 *   as published by the Free Software Foundation; either version 2 of     *
 *   the License, or (at your option) any later version.                   *
 *   for detail see the LICENCE text file.                                 *
 *                                                                         *
 *   FreeCAD is distributed in the hope that it will be useful,            *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        * 
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU Library General Public License for more details.                  *
 *                                                                         *
 *   You should have received a copy of the GNU Library General Public     *
 *   License along with FreeCAD; if not, write to the Free Software        * 
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  *
 *   USA                                                                   *
 *                                                                         *
 *   Juergen Riegel 2002                                                   *
 ***************************************************************************/
#include <FCConfig.h>

#ifdef _PreComp_
#   undef _PreComp_
#endif

#ifdef FC_OS_LINUX
#   include <unistd.h>
#endif

#if HAVE_CONFIG_H
#   include <config.h>
#endif // HAVE_CONFIG_H

#include <map>
#include <vector>
#include <algorithm>

#include <cstdio>
#include <QApplication>
#include <QFile>
#include <QMessageBox>
#include <QLocale>
#include <QTextCodec>

// FreeCAD header
#include <Base/Console.h>
#include <Base/Interpreter.h>
#include <Base/Parameter.h>
#include <Base/Exception.h>
#include <Base/Factory.h>
#include <App/Application.h>
#include <Gui/BitmapFactory.h>
#include <Gui/Application.h>

void PrintInitHelp(void);

const char sBanner[] = "\xc2\xa9 Juergen Riegel, Werner Mayer, Yorik van Havre 2001-2015\n"\
"  #####                 ####  ###   ####  \n" \
"  #                    #      # #   #   # \n" \
"  #     ##  #### ####  #     #   #  #   # \n" \
"  ####  # # #  # #  #  #     #####  #   # \n" \
"  #     #   #### ####  #    #     # #   # \n" \
"  #     #   #    #     #    #     # #   #  ##  ##  ##\n" \
"  #     #   #### ####   ### #     # ####   ##  ##  ##\n\n" ;

#if defined(_MSC_VER)
void InitMiniDumpWriter(const std::string&);
#endif

#if defined (FC_OS_LINUX) || defined(FC_OS_BSD)
QString myDecoderFunc(const QByteArray &localFileName)
{
    QTextCodec* codec = QTextCodec::codecForName("UTF-8");
    return codec->toUnicode(localFileName);
}

QByteArray myEncoderFunc(const QString &fileName)
{
    QTextCodec* codec = QTextCodec::codecForName("UTF-8");
    return codec->fromUnicode(fileName);
}
#endif

int main( int argc, char ** argv )
{
#if defined (FC_OS_LINUX) || defined(FC_OS_BSD)
    // Make sure to setup the Qt locale system before setting LANG and LC_ALL to C.
    // which is needed to use the system locale settings.
    (void)QLocale::system();
    // http://www.freecadweb.org/tracker/view.php?id=399
    // Because of setting LANG=C the Qt automagic to use the correct encoding
    // for file names is broken. This is a workaround to force the use of UTF-8 encoding
    QFile::setEncodingFunction(myEncoderFunc);
    QFile::setDecodingFunction(myDecoderFunc);
    // Make sure that we use '.' as decimal point. See also
    // http://bugs.debian.org/cgi-bin/bugreport.cgi?bug=559846
    putenv("LC_NUMERIC=C");
    putenv("PYTHONPATH=");
#elif defined(FC_OS_MACOSX)
    (void)QLocale::system();
    putenv("LC_NUMERIC=C");
    putenv("PYTHONPATH=");
#else
    setlocale(LC_NUMERIC, "C");
    _putenv("PYTHONPATH=");
#endif

#if defined (FC_OS_WIN32)
    int argc_ = argc;
    QVector<QByteArray> data;
    QVector<char *> argv_;

    // get the command line arguments as unicode string
    {
        QCoreApplication app(argc, argv);
        QStringList args = app.arguments();
        for (QStringList::iterator it = args.begin(); it != args.end(); ++it) {
            data.push_back(it->toUtf8());
            argv_.push_back(data.back().data());
        }
        argv_.push_back(0); // 0-terminated string
    }
#endif

    // Name and Version of the Application
    App::Application::Config()["ExeName"] = "FreeCAD";
    App::Application::Config()["ExeVendor"] = "FreeCAD";
    App::Application::Config()["AppDataSkipVendor"] = "true";
    App::Application::Config()["MaintainerUrl"] = "http://www.freecadweb.org/wiki/index.php?title=Main_Page";

    // set the banner (for logging and console)
    App::Application::Config()["CopyrightInfo"] = sBanner;
    App::Application::Config()["AppIcon"] = "freecad";
    App::Application::Config()["SplashScreen"] = "freecadsplash";
    App::Application::Config()["StartWorkbench"] = "StartWorkbench";
    //App::Application::Config()["HiddenDockWindow"] = "Property editor";
    App::Application::Config()["SplashAlignment" ] = "Bottom|Left";
    App::Application::Config()["SplashTextColor" ] = "#ffffff"; // white
    App::Application::Config()["SplashInfoColor" ] = "#c8c8c8"; // light grey

    try {
        // Init phase ===========================================================
        // sets the default run mode for FC, starts with gui if not overridden in InitConfig...
        App::Application::Config()["RunMode"] = "Gui";

        // Inits the Application 
#if defined (FC_OS_WIN32)
        App::Application::init(argc_, argv_.data());
#else
        App::Application::init(argc, argv);
#endif
#if defined(_MSC_VER)
        // create a dump file when the application crashes
        std::string dmpfile = App::Application::getUserAppDataDir();
        dmpfile += "crash.dmp";
        InitMiniDumpWriter(dmpfile);
#endif
        std::map<std::string, std::string>::iterator it = App::Application::Config().find("NavigationStyle");
        if (it != App::Application::Config().end()) {
            ParameterGrp::handle hGrp = App::GetApplication().GetParameterGroupByPath("User parameter:BaseApp/Preferences/View");
            // if not already defined do it now (for the very first start)
            std::string style = hGrp->GetASCII("NavigationStyle", it->second.c_str());
            hGrp->SetASCII("NavigationStyle", style.c_str());
        }

        Gui::Application::initApplication();
        Base::Interpreter().replaceStdOutput();
    }
    catch (const Base::UnknownProgramOption& e) {
        QApplication app(argc,argv);
        QString appName = QString::fromAscii(App::Application::Config()["ExeName"].c_str());
        QString msg = QString::fromAscii(e.what());
        QString s = QLatin1String("<pre>") + msg + QLatin1String("</pre>");
        QMessageBox::critical(0, appName, s);
        exit(1);
    }
    catch (const Base::ProgramInformation& e) {
        QApplication app(argc,argv);
        QString appName = QString::fromAscii(App::Application::Config()["ExeName"].c_str());
        QString msg = QString::fromAscii(e.what());
        QString s = QLatin1String("<pre>") + msg + QLatin1String("</pre>");
        QMessageBox::information(0, appName, s);
        exit(0);
    }
    catch (const Base::Exception& e) {
        // Popup an own dialog box instead of that one of Windows
        QApplication app(argc,argv);
        QString appName = QString::fromAscii(App::Application::Config()["ExeName"].c_str());
        QString msg;
        msg = QObject::tr("While initializing %1 the  following exception occurred: '%2'\n\n"
                          "Python is searching for its files in the following directories:\n%3\n\n"
                          "Python version information:\n%4\n")
                          .arg(appName).arg(QString::fromUtf8(e.what()))
                          .arg(QString::fromUtf8(Py_GetPath())).arg(QString::fromAscii(Py_GetVersion()));
        const char* pythonhome = getenv("PYTHONHOME");
        if (pythonhome) {
            msg += QObject::tr("\nThe environment variable PYTHONHOME is set to '%1'.")
                .arg(QString::fromUtf8(pythonhome));
            msg += QObject::tr("\nSetting this environment variable might cause Python to fail. "
                "Please contact your administrator to unset it on your system.\n\n");
        } else {
            msg += QObject::tr("\nPlease contact the application's support team for more information.\n\n");
        }

        QMessageBox::critical(0, QObject::tr("Initialization of %1 failed").arg(appName), msg);
        exit(100);
    }
    catch (...) {
        // Popup an own dialog box instead of that one of Windows
        QApplication app(argc,argv);
        QString appName = QString::fromAscii(App::Application::Config()["ExeName"].c_str());
        QString msg = QObject::tr("Unknown runtime error occurred while initializing %1.\n\n"
                                  "Please contact the application's support team for more information.\n\n").arg(appName);
        QMessageBox::critical(0, QObject::tr("Initialization of %1 failed").arg(appName), msg);
        exit(101);
    }

    // Run phase ===========================================================
    Base::RedirectStdOutput stdcout;
    Base::RedirectStdLog    stdclog;
    Base::RedirectStdError  stdcerr;
    std::streambuf* oldcout = std::cout.rdbuf(&stdcout);
    std::streambuf* oldclog = std::clog.rdbuf(&stdclog);
    std::streambuf* oldcerr = std::cerr.rdbuf(&stdcerr);

    try {
        if (App::Application::Config()["RunMode"] == "Gui")
            Gui::Application::runApplication();
        else
            App::Application::runApplication();
    }
    catch (const Base::SystemExitException&) {
        exit(0);
    }
    catch (const Base::Exception& e) {
        Base::Console().Error("%s\n", e.what());
    }
    catch (...) {
        Base::Console().Error("Application unexpectedly terminated\n");
    }

    std::cout.rdbuf(oldcout);
    std::clog.rdbuf(oldclog);
    std::cerr.rdbuf(oldcerr);

    // Destruction phase ===========================================================
    Base::Console().Log("%s terminating...\n",App::Application::Config()["ExeName"].c_str());

    // cleans up 
    App::Application::destruct();

    Base::Console().Log("%s completely terminated\n",App::Application::Config()["ExeName"].c_str());

    return 0;
}

#if defined(_MSC_VER)
#include <windows.h>
#include <dbghelp.h>

typedef BOOL (__stdcall *tMDWD)(
  IN HANDLE hProcess,
  IN DWORD ProcessId,
  IN HANDLE hFile,
  IN MINIDUMP_TYPE DumpType,
  IN CONST PMINIDUMP_EXCEPTION_INFORMATION ExceptionParam, OPTIONAL
  IN CONST PMINIDUMP_USER_STREAM_INFORMATION UserStreamParam, OPTIONAL
  IN CONST PMINIDUMP_CALLBACK_INFORMATION CallbackParam OPTIONAL
  );

static tMDWD s_pMDWD;
static HMODULE s_hDbgHelpMod;
static MINIDUMP_TYPE s_dumpTyp = MiniDumpNormal;
static std::string s_szMiniDumpFileName;  // initialize with whatever appropriate...

#include <Base/StackWalker.h>
class MyStackWalker : public StackWalker
{
    DWORD threadId;
public:
    MyStackWalker() : StackWalker(), threadId(GetCurrentThreadId())
    {
        std::string name = App::Application::Config()["UserAppData"] + "crash.log";
        Base::Console().AttachObserver(new Base::ConsoleObserverFile(name.c_str()));
    }
    MyStackWalker(DWORD dwProcessId, HANDLE hProcess) : StackWalker(dwProcessId, hProcess) {}
    virtual void OnOutput(LPCSTR szText)
    {
        Base::Console().Log("Id: %ld: %s", threadId, szText);
        //StackWalker::OnOutput(szText);
    }
};

static LONG __stdcall MyCrashHandlerExceptionFilter(EXCEPTION_POINTERS* pEx) 
{
#ifdef _M_IX86 
  if (pEx->ExceptionRecord->ExceptionCode == EXCEPTION_STACK_OVERFLOW)   
  { 
    // be sure that we have enought space... 
    static char MyStack[1024*128];   
    // it assumes that DS and SS are the same!!! (this is the case for Win32) 
    // change the stack only if the selectors are the same (this is the case for Win32) 
    //__asm push offset MyStack[1024*128]; 
    //__asm pop esp; 
    __asm mov eax,offset MyStack[1024*128]; 
    __asm mov esp,eax; 
  } 
#endif 
  MyStackWalker sw;
  sw.ShowCallstack(GetCurrentThread(), pEx->ContextRecord);
  Base::Console().Log("*** Unhandled Exception!\n");
  Base::Console().Log("   ExpCode: 0x%8.8X\n", pEx->ExceptionRecord->ExceptionCode);
  Base::Console().Log("   ExpFlags: %d\n", pEx->ExceptionRecord->ExceptionFlags);
  Base::Console().Log("   ExpAddress: 0x%8.8X\n", pEx->ExceptionRecord->ExceptionAddress);

  bool bFailed = true; 
  HANDLE hFile; 
  hFile = CreateFile(s_szMiniDumpFileName.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL); 
  if (hFile != INVALID_HANDLE_VALUE) 
  { 
    MINIDUMP_EXCEPTION_INFORMATION stMDEI; 
    stMDEI.ThreadId = GetCurrentThreadId(); 
    stMDEI.ExceptionPointers = pEx; 
    stMDEI.ClientPointers = TRUE; 
    // try to create an miniDump: 
    if (s_pMDWD( 
      GetCurrentProcess(), 
      GetCurrentProcessId(), 
      hFile, 
      s_dumpTyp, 
      &stMDEI, 
      NULL, 
      NULL 
      )) 
    { 
      bFailed = false;  // suceeded 
    } 
    CloseHandle(hFile); 
  } 

  if (bFailed) 
  { 
    return EXCEPTION_CONTINUE_SEARCH; 
  } 

  // Optional display an error message 
  // FatalAppExit(-1, ("Application failed!")); 


  // or return one of the following: 
  // - EXCEPTION_CONTINUE_SEARCH 
  // - EXCEPTION_CONTINUE_EXECUTION 
  // - EXCEPTION_EXECUTE_HANDLER 
  return EXCEPTION_CONTINUE_SEARCH;  // this will trigger the "normal" OS error-dialog 
} 

void InitMiniDumpWriter(const std::string& filename)
{
  if (s_hDbgHelpMod != NULL)
    return;
  s_szMiniDumpFileName = filename;

  // Initialize the member, so we do not load the dll after the exception has occured
  // which might be not possible anymore...
  s_hDbgHelpMod = LoadLibrary(("dbghelp.dll"));
  if (s_hDbgHelpMod != NULL)
    s_pMDWD = (tMDWD) GetProcAddress(s_hDbgHelpMod, "MiniDumpWriteDump");

  // Register Unhandled Exception-Filter:
  SetUnhandledExceptionFilter(MyCrashHandlerExceptionFilter);

  // Additional call "PreventSetUnhandledExceptionFilter"...
  // See also: "SetUnhandledExceptionFilter" and VC8 (and later)
  // http://blog.kalmbachnet.de/?postid=75
}
#endif
