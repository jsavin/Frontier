# Frontier Verb Implementation Status Report

**Generated**: Programmatic analysis from kernelverbs.rc

## Executive Summary

- **Total Processors**: 51
- **Total Verbs**: 707
- **Fully Implemented**: 0
- **Platform-Specific**: 0
- **GUI-Dependent (Headless)**: 0
- **Missing Implementations**: 234
- **Unimplemented Stubs**: 473

## Implementation Categories

### Category 1: Unimplemented Stubs (Ready for Phase 2 Testing)

**Count**: 473 verbs across multiple stub processors

These processors have been generated but contain only stub implementations that return
"not implemented". This is by design - they are ready for systematic testing to discover
which verbs work in headless mode and which need special handling.

**Processors**:

- **base64**: 2 verbs (stub)
- **bit**: 8 verbs (stub)
- **clipboard**: 2 verbs (stub)
- **clock**: 7 verbs (stub)
- **crypt**: 5 verbs (stub)
- **db**: 13 verbs (stub)
- **dll**: 4 verbs (stub)
- **editmenu**: 16 verbs (stub)
- **file**: 86 verbs (stub)
- **filemenu**: 10 verbs (stub)
- **frontier**: 14 verbs (stub)
- **html**: 23 verbs (stub)
- **htmlcontrol**: 8 verbs (stub)
- **inetd**: 1 verbs (stub)
- **kb**: 4 verbs (stub)
- **lang**: 58 verbs (stub)
- **launch**: 5 verbs (stub)
- **mainwindow**: 7 verbs (stub)
- **mouse**: 2 verbs (stub)
- **mrcalendar**: 11 verbs (stub)
- **mysql**: 27 verbs (stub)
- **opattributes**: 5 verbs (stub)
- **osa**: 2 verbs (stub)
- **pict**: 4 verbs (stub)
- **point**: 2 verbs (stub)
- **python**: 1 verbs (stub)
- **re**: 10 verbs (stub)
- **rectangle**: 2 verbs (stub)
- **rez**: 15 verbs (stub)
- **rgb**: 2 verbs (stub)
- **script**: 13 verbs (stub)
- **search**: 6 verbs (stub)
- **searchengine**: 5 verbs (stub)
- **semaphore**: 2 verbs (stub)
- **speaker**: 3 verbs (stub)
- **sqlite**: 17 verbs (stub)
- **statusbar**: 5 verbs (stub)
- **sys**: 16 verbs (stub)
- **target**: 3 verbs (stub)
- **tcp**: 23 verbs (stub)
- **thread**: 17 verbs (stub)
- **webserver**: 7 verbs (stub)

### Category 2: Platform-Specific Verbs

**Count**: 0 verbs

These verbs have conditional compilation guards (#ifdef WIN32, #ifdef MAC, etc.).
They need platform-specific implementations for each supported OS.

**Verbs**:


### Category 3: GUI-Dependent Verbs (Headless Incompatible)

**Count**: 0 verbs

These verbs are guarded with #ifdef FRONTIER_HEADLESS or similar. They likely require
UI features that are not available in headless mode and may not be implementable.

**Verbs**:


### Category 4: Missing Implementations

**Count**: 234 verbs

These verbs are defined but not yet implemented (likely stubs or TODOs).

**Verbs**:

- date.abbrevstring
- date.day
- date.dayofweek
- date.dayofweektostring
- date.daysinmonth
- date.daystring
- date.firstofmonth
- date.get
- date.getcurrenttimezone
- date.hour
- date.lastofmonth
- date.longstring
- date.minute
- date.month
- date.monthtostring
- date.netstandardstring
- date.nextmonth
- date.nextweek
- date.nextyear
- date.prevmonth
- date.prevweek
- date.prevyear
- date.seconds
- date.set
- date.shortstring
- date.tomorrow
- date.versionlessthan
- date.weeksinmonth
- date.year
- date.yesterday
- dialog.alert
- dialog.ask
- dialog.getint
- dialog.getpassword
- dialog.getuserinfo
- dialog.getvalue
- dialog.hideitem
- dialog.ismodalcard
- dialog.notify
- dialog.run
- dialog.runcard
- dialog.runmodalcard
- dialog.runmodeless
- dialog.setitemenable
- dialog.setmodalcardtimeout
- dialog.setvalue
- dialog.showitem
- dialog.threeway
- dialog.twoway
- math.max
- math.min
- math.sqrt
- menu.addmenucommand
- menu.addsubmenu
- menu.buildmenubar
- menu.clearmenubar
- menu.deletemenucommand
- menu.deletesubmenu
- menu.getcommandkey
- menu.getscript
- menu.install
- menu.isinstalled
- menu.remove
- menu.setcommandkey
- menu.setscript
- menu.zoomscript
- op.collapse
- op.countsubs
- op.countsummits
- op.dehoist
- op.deleteline
- op.deletesubs
- op.demote
- op.expand
- op.find
- op.firstsummit
- op.flatcursorkeys
- op.getcursor
- op.getdisplay
- op.getdynamic
- op.getexpansionstate
- op.getheadnumber
- op.gethtmlformatting
- op.getlinetext
- op.getrefcon
- op.getscrollstate
- op.getselectedsuboutlines
- op.getselection
- op.getsuboutline
- op.go
- op.hoist
- op.insert
- op.insertoutline
- op.level
- op.outlinetoxml
- op.promote
- op.reorg
- op.setcursor
- op.setdisplay
- op.setdynamic
- op.setexpansionstate
- op.sethtmlformatting
- op.setlinetext
- op.setmodified
- op.setrefcon
- op.setscrollstate
- op.sort
- op.subsexpanded
- op.tabkeyreorg
- op.visitall
- op.xmltooutline
- string.addcommas
- string.ansitoutf16
- string.ansitoutf8
- string.commentdelete
- string.convertcharset
- string.countfields
- string.countwords
- string.datestring
- string.davenetmassager
- string.delete
- string.dropnonalphas
- string.ellipsize
- string.filledstring
- string.firstsentence
- string.firstword
- string.getgifheightwidth
- string.getjpegheightwidth
- string.getwordchar
- string.hashMD5
- string.hassuffix
- string.hex
- string.innercasename
- string.insert
- string.isalpha
- string.ischarsetavailable
- string.isnumeric
- string.iso8859encode
- string.ispunctuation
- string.lastword
- string.latintomac
- string.length
- string.lower
- string.macromantoutf8
- string.mactolatin
- string.mid
- string.multiplereplaceall
- string.nthchar
- string.nthfield
- string.nthword
- string.padwithzeros
- string.parseaddress
- string.parsehttpargs
- string.patternmatch
- string.popleading
- string.popsuffix
- string.poptrailing
- string.processhtmlmacros
- string.replace
- string.replaceall
- string.setwordchar
- string.timestring
- string.trimwhitespace
- string.upper
- string.urldecode
- string.urlencode
- string.urlsplit
- string.utf16toansi
- string.utf8toansi
- string.utf8tomacroman
- string.wrap
- table.assign
- table.copy
- table.emptytable
- table.getcursor
- table.getdisplaysettings
- table.getselection
- table.getsortorder
- table.go
- table.goto
- table.gotoname
- table.jettison
- table.move
- table.moveandrename
- table.packtable
- table.rename
- table.setdisplaysettings
- table.sortby
- table.validate
- window.about
- window.bringtofront
- window.close
- window.dbstats
- window.frontmost
- window.getfile
- window.getposition
- window.getsize
- window.gettitle
- window.hide
- window.isfront
- window.ismenuscript
- window.ismodified
- window.isopen
- window.isreadonly
- window.isvisible
- window.msg
- window.next
- window.open
- window.quickscript
- window.runselection
- window.scroll
- window.sendtoback
- window.setmodified
- window.setposition
- window.setquickscript
- window.setsize
- window.settitle
- window.show
- window.update
- window.zoom
- xml.addtable
- xml.addvalue
- xml.compile
- xml.converttodisplayname
- xml.decompile
- xml.frontiervaluetotaggedtext
- xml.getaddress
- xml.getaddresslist
- xml.getattribute
- xml.getattributevalue
- xml.getpathaddress
- xml.getvalue
- xml.structtofrontiervalue
- xml.valtostring

### Category 5: Fully Implemented Verbs

**Count**: 0 verbs

These verbs have complete, working implementations.

**Verbs**:


## Verb Details by Processor

Complete listing of all verbs organized by processor:

### base64 (2 verbs)

- `encode` - unimplemented_stub
- `decode` - unimplemented_stub

### bit (8 verbs)

- `get` - unimplemented_stub
- `set` - unimplemented_stub
- `clear` - unimplemented_stub
- `logicaland` - unimplemented_stub
- `logicalor` - unimplemented_stub
- `logicalxor` - unimplemented_stub
- `shiftleft` - unimplemented_stub
- `shiftright` - unimplemented_stub

### clipboard (2 verbs)

- `get` - unimplemented_stub
- `put` - unimplemented_stub

### clock (7 verbs)

- `now` - unimplemented_stub
- `set` - unimplemented_stub
- `sleepfor` - unimplemented_stub
- `ticks` - unimplemented_stub
- `milliseconds` - unimplemented_stub
- `waitseconds` - unimplemented_stub
- `waitsixtieths` - unimplemented_stub

### crypt (5 verbs)

- `whirlpool` - unimplemented_stub
- `hmacMD5` - unimplemented_stub
- `MD5` - unimplemented_stub
- `SHA1` - unimplemented_stub
- `hmacSHA1` - unimplemented_stub

### date (30 verbs)

- `get` - missing
- `set` - missing
- `abbrevstring` - missing
- `dayofweek` - missing
- `daysinmonth` - missing
- `daystring` - missing
- `firstofmonth` - missing
- `lastofmonth` - missing
- `longstring` - missing
- `nextmonth` - missing
- `nextweek` - missing
- `nextyear` - missing
- `prevmonth` - missing
- `prevweek` - missing
- `prevyear` - missing
- `shortstring` - missing
- `tomorrow` - missing
- `weeksinmonth` - missing
- `yesterday` - missing
- `getcurrenttimezone` - missing
- `netstandardstring` - missing
- `monthtostring` - missing
- `dayofweektostring` - missing
- `versionlessthan` - missing
- `day` - missing
- `month` - missing
- `year` - missing
- `hour` - missing
- `minute` - missing
- `seconds` - missing

### db (13 verbs)

- `new` - unimplemented_stub
- `open` - unimplemented_stub
- `save` - unimplemented_stub
- `close` - unimplemented_stub
- `defined` - unimplemented_stub
- `getvalue` - unimplemented_stub
- `setvalue` - unimplemented_stub
- `delete` - unimplemented_stub
- `newTable` - unimplemented_stub
- `isTable` - unimplemented_stub
- `countitems` - unimplemented_stub
- `getnthitem` - unimplemented_stub
- `getmoddate` - unimplemented_stub

### dialog (19 verbs)

- `alert` - missing
- `run` - missing
- `runmodeless` - missing
- `runcard` - missing
- `runmodalcard` - missing
- `ismodalcard` - missing
- `setmodalcardtimeout` - missing
- `getvalue` - missing
- `setvalue` - missing
- `setitemenable` - missing
- `showitem` - missing
- `hideitem` - missing
- `twoway` - missing
- `threeway` - missing
- `ask` - missing
- `getint` - missing
- `notify` - missing
- `getuserinfo` - missing
- `getpassword` - missing

### dll (4 verbs)

- `call` - unimplemented_stub
- `load` - unimplemented_stub
- `unload` - unimplemented_stub
- `isloaded` - unimplemented_stub

### editmenu (16 verbs)

- `undo` - unimplemented_stub
- `cut` - unimplemented_stub
- `copy` - unimplemented_stub
- `paste` - unimplemented_stub
- `clear` - unimplemented_stub
- `selectall` - unimplemented_stub
- `getfont` - unimplemented_stub
- `getfontsize` - unimplemented_stub
- `setfont` - unimplemented_stub
- `setfontsize` - unimplemented_stub
- `plaintext` - unimplemented_stub
- `setbold` - unimplemented_stub
- `setitalic` - unimplemented_stub
- `setunderline` - unimplemented_stub
- `setoutline` - unimplemented_stub
- `setshadow` - unimplemented_stub

### file (86 verbs)

- `created` - unimplemented_stub
- `modified` - unimplemented_stub
- `type` - unimplemented_stub
- `creator` - unimplemented_stub
- `setcreated` - unimplemented_stub
- `setmodified` - unimplemented_stub
- `settype` - unimplemented_stub
- `setcreator` - unimplemented_stub
- `isfolder` - unimplemented_stub
- `isvolume` - unimplemented_stub
- `islocked` - unimplemented_stub
- `lock` - unimplemented_stub
- `unlock` - unimplemented_stub
- `copy` - unimplemented_stub
- `copydatafork` - unimplemented_stub
- `copyresourcefork` - unimplemented_stub
- `delete` - unimplemented_stub
- `rename` - unimplemented_stub
- `exists` - unimplemented_stub
- `size` - unimplemented_stub
- `fullpath` - unimplemented_stub
- `getpath` - unimplemented_stub
- `setpath` - unimplemented_stub
- `filefrompath` - unimplemented_stub
- `folderfrompath` - unimplemented_stub
- `getsystemfolderpath` - unimplemented_stub
- `getspecialfolderpath` - unimplemented_stub
- `new` - unimplemented_stub
- `newfolder` - unimplemented_stub
- `newalias` - unimplemented_stub
- `getfiledialog` - unimplemented_stub
- `putfiledialog` - unimplemented_stub
- `getfolderdialog` - unimplemented_stub
- `getdiskdialog` - unimplemented_stub
- `geticonpos` - unimplemented_stub
- `seticonpos` - unimplemented_stub
- `getversion` - unimplemented_stub
- `setversion` - unimplemented_stub
- `getfullversion` - unimplemented_stub
- `setfullversion` - unimplemented_stub
- `getcomment` - unimplemented_stub
- `setcomment` - unimplemented_stub
- `getlabel` - unimplemented_stub
- `setlabel` - unimplemented_stub
- `findapplication` - unimplemented_stub
- `isbusy` - unimplemented_stub
- `hasbundle` - unimplemented_stub
- `setbundle` - unimplemented_stub
- `isalias` - unimplemented_stub
- `isvisible` - unimplemented_stub
- `setvisible` - unimplemented_stub
- `followalias` - unimplemented_stub
- `move` - unimplemented_stub
- `eject` - unimplemented_stub
- `isejectable` - unimplemented_stub
- `freespaceonvolume` - unimplemented_stub
- `volumesize` - unimplemented_stub
- `volumeblocksize` - unimplemented_stub
- `filesonvolume` - unimplemented_stub
- `foldersonvolume` - unimplemented_stub
- `unmountvolume` - unimplemented_stub
- `mountservervolume` - unimplemented_stub
- `findinfile` - unimplemented_stub
- `countlines` - unimplemented_stub
- `open` - unimplemented_stub
- `close` - unimplemented_stub
- `endoffile` - unimplemented_stub
- `setendoffile` - unimplemented_stub
- `getendoffile` - unimplemented_stub
- `setposition` - unimplemented_stub
- `getposition` - unimplemented_stub
- `readline` - unimplemented_stub
- `writeline` - unimplemented_stub
- `read` - unimplemented_stub
- `write` - unimplemented_stub
- `compare` - unimplemented_stub
- `writewholefile` - unimplemented_stub
- `getpathchar` - unimplemented_stub
- `freespaceonvolumedouble` - unimplemented_stub
- `volumesizedouble` - unimplemented_stub
- `getmp3info` - unimplemented_stub
- `readwholefile` - unimplemented_stub
- `getLabelIndex` - unimplemented_stub
- `setLabelIndex` - unimplemented_stub
- `getLabelNames` - unimplemented_stub
- `getPosixPath` - unimplemented_stub

### filemenu (10 verbs)

- `new` - unimplemented_stub
- `open` - unimplemented_stub
- `close` - unimplemented_stub
- `closeall` - unimplemented_stub
- `save` - unimplemented_stub
- `savecopy` - unimplemented_stub
- `revert` - unimplemented_stub
- `print` - unimplemented_stub
- `quit` - unimplemented_stub
- `saveas` - unimplemented_stub

### frontier (14 verbs)

- `getprogrampath` - unimplemented_stub
- `getfilepath` - unimplemented_stub
- `enableagents` - unimplemented_stub
- `requesttofront` - unimplemented_stub
- `isruntime` - unimplemented_stub
- `countthreads` - unimplemented_stub
- `ispowerpc` - unimplemented_stub
- `reclaimmemory` - unimplemented_stub
- `version` - unimplemented_stub
- `hashstats` - unimplemented_stub
- `gethashloopcount` - unimplemented_stub
- `hideapplication` - unimplemented_stub
- `isvalidserialnumber` - unimplemented_stub
- `showapplication` - unimplemented_stub

### html (23 verbs)

- `processmacros` - unimplemented_stub
- `urldecode` - unimplemented_stub
- `urlencode` - unimplemented_stub
- `parsehttpargs` - unimplemented_stub
- `iso8859encode` - unimplemented_stub
- `getgifheightwidth` - unimplemented_stub
- `getjpegheightwidth` - unimplemented_stub
- `buildpagetable` - unimplemented_stub
- `refglossary` - unimplemented_stub
- `getpref` - unimplemented_stub
- `getonedirective` - unimplemented_stub
- `rundirective` - unimplemented_stub
- `rundirectives` - unimplemented_stub
- `runoutlinedirectives` - unimplemented_stub
- `cleanforexport` - unimplemented_stub
- `normalizename` - unimplemented_stub
- `glossarypatcher` - unimplemented_stub
- `expandurls` - unimplemented_stub
- `traversalskip` - unimplemented_stub
- `getpagetableaddress` - unimplemented_stub
- `neutermacros` - unimplemented_stub
- `neutertags` - unimplemented_stub
- `drawcalendar` - unimplemented_stub

### htmlcontrol (8 verbs)

- `back` - unimplemented_stub
- `forward` - unimplemented_stub
- `refresh` - unimplemented_stub
- `home` - unimplemented_stub
- `stop` - unimplemented_stub
- `navigate` - unimplemented_stub
- `isoffline` - unimplemented_stub
- `setoffline` - unimplemented_stub

### inetd (1 verbs)

- `supervisor` - unimplemented_stub

### kb (4 verbs)

- `optionkey` - unimplemented_stub
- `cmdkey` - unimplemented_stub
- `shiftkey` - unimplemented_stub
- `controlkey` - unimplemented_stub

### lang (58 verbs)

- `scripterror` - unimplemented_stub
- `new` - unimplemented_stub
- `delete` - unimplemented_stub
- `edit` - unimplemented_stub
- `close` - unimplemented_stub
- `timecreated` - unimplemented_stub
- `timemodified` - unimplemented_stub
- `settimecreated` - unimplemented_stub
- `settimemodified` - unimplemented_stub
- `boolean` - unimplemented_stub
- `char` - unimplemented_stub
- `short` - unimplemented_stub
- `long` - unimplemented_stub
- `date` - unimplemented_stub
- `direction` - unimplemented_stub
- `string4` - unimplemented_stub
- `string` - unimplemented_stub
- `displaystring` - unimplemented_stub
- `address` - unimplemented_stub
- `binary` - unimplemented_stub
- `getbinarytype` - unimplemented_stub
- `setbinarytype` - unimplemented_stub
- `point` - unimplemented_stub
- `rect` - unimplemented_stub
- `rgb` - unimplemented_stub
- `pattern` - unimplemented_stub
- `fixed` - unimplemented_stub
- `single` - unimplemented_stub
- `double` - unimplemented_stub
- `filespec` - unimplemented_stub
- `alias` - unimplemented_stub
- `list` - unimplemented_stub
- `record` - unimplemented_stub
- `enum` - unimplemented_stub
- `memavail` - unimplemented_stub
- `flushmemory` - unimplemented_stub
- `random` - unimplemented_stub
- `evaluate` - unimplemented_stub
- `evaluatethread` - unimplemented_stub
- `rollbeachball` - unimplemented_stub
- `abs` - unimplemented_stub
- `seteventtimeout` - unimplemented_stub
- `seteventtransactionid` - unimplemented_stub
- `seteventinteraction` - unimplemented_stub
- `geteventattribute` - unimplemented_stub
- `coerceappleitem` - unimplemented_stub
- `getapplelistitem` - unimplemented_stub
- `putapplelistitem` - unimplemented_stub
- `countapplelistitems` - unimplemented_stub
- `systemevent` - unimplemented_stub
- `DDEevent` - unimplemented_stub
- `transactionEvent` - unimplemented_stub
- `msg` - unimplemented_stub
- `callxcmd` - unimplemented_stub
- `calldll` - unimplemented_stub
- `packwindow` - unimplemented_stub
- `unpackwindow` - unimplemented_stub
- `callscript` - unimplemented_stub

### launch (5 verbs)

- `applemenu` - unimplemented_stub
- `application` - unimplemented_stub
- `appwithdocument` - unimplemented_stub
- `resource` - unimplemented_stub
- `anything` - unimplemented_stub

### mainwindow (7 verbs)

- `showflag` - unimplemented_stub
- `hideflag` - unimplemented_stub
- `showpopup` - unimplemented_stub
- `hidepopup` - unimplemented_stub
- `showbuttons` - unimplemented_stub
- `hidebuttons` - unimplemented_stub
- `showserverstats` - unimplemented_stub

### math (3 verbs)

- `min` - missing
- `max` - missing
- `sqrt` - missing

### menu (14 verbs)

- `zoomscript` - missing
- `buildmenubar` - missing
- `clearmenubar` - missing
- `isinstalled` - missing
- `install` - missing
- `remove` - missing
- `getscript` - missing
- `setscript` - missing
- `addmenucommand` - missing
- `deletemenucommand` - missing
- `addsubmenu` - missing
- `deletesubmenu` - missing
- `getcommandkey` - missing
- `setcommandkey` - missing

### mouse (2 verbs)

- `button` - unimplemented_stub
- `location` - unimplemented_stub

### mrcalendar (11 verbs)

- `getaddressday` - unimplemented_stub
- `getdayaddress` - unimplemented_stub
- `getfirstaddress` - unimplemented_stub
- `getfirstday` - unimplemented_stub
- `getlastaddress` - unimplemented_stub
- `getlastday` - unimplemented_stub
- `getmostrecentaddress` - unimplemented_stub
- `getmostrecentday` - unimplemented_stub
- `getnextaddress` - unimplemented_stub
- `getnextday` - unimplemented_stub
- `navigate` - unimplemented_stub

### mysql (27 verbs)

- `init` - unimplemented_stub
- `end` - unimplemented_stub
- `connect` - unimplemented_stub
- `compileQuery` - unimplemented_stub
- `clearQuery` - unimplemented_stub
- `getRow` - unimplemented_stub
- `getErrorNumber` - unimplemented_stub
- `getErrorMessage` - unimplemented_stub
- `getClientInfo` - unimplemented_stub
- `getClientVersion` - unimplemented_stub
- `getHostInfo` - unimplemented_stub
- `getServerVersion` - unimplemented_stub
- `getProtocolInfo` - unimplemented_stub
- `getServerInfo` - unimplemented_stub
- `getQueryInfo` - unimplemented_stub
- `getAffectedRowCount` - unimplemented_stub
- `getSelectedRowCount` - unimplemented_stub
- `getColumnCount` - unimplemented_stub
- `getServerStatus` - unimplemented_stub
- `getQueryWarningCount` - unimplemented_stub
- `pingServer` - unimplemented_stub
- `seekRow` - unimplemented_stub
- `selectDatabase` - unimplemented_stub
- `getSQLSTATE` - unimplemented_stub
- `escapeString` - unimplemented_stub
- `isThreadSafe` - unimplemented_stub
- `close` - unimplemented_stub

### op (45 verbs)

- `getlinetext` - missing
- `level` - missing
- `countsubs` - missing
- `countsummits` - missing
- `go` - missing
- `firstsummit` - missing
- `expand` - missing
- `collapse` - missing
- `subsexpanded` - missing
- `insert` - missing
- `find` - missing
- `sort` - missing
- `setlinetext` - missing
- `reorg` - missing
- `promote` - missing
- `demote` - missing
- `hoist` - missing
- `dehoist` - missing
- `deletesubs` - missing
- `deleteline` - missing
- `tabkeyreorg` - missing
- `flatcursorkeys` - missing
- `getdisplay` - missing
- `setdisplay` - missing
- `getcursor` - missing
- `setcursor` - missing
- `getrefcon` - missing
- `setrefcon` - missing
- `getexpansionstate` - missing
- `setexpansionstate` - missing
- `getscrollstate` - missing
- `setscrollstate` - missing
- `getsuboutline` - missing
- `insertoutline` - missing
- `setmodified` - missing
- `getselection` - missing
- `getheadnumber` - missing
- `visitall` - missing
- `getselectedsuboutlines` - missing
- `xmltooutline` - missing
- `outlinetoxml` - missing
- `sethtmlformatting` - missing
- `gethtmlformatting` - missing
- `setdynamic` - missing
- `getdynamic` - missing

### opattributes (5 verbs)

- `addgroup` - unimplemented_stub
- `getall` - unimplemented_stub
- `getone` - unimplemented_stub
- `makeempty` - unimplemented_stub
- `setone` - unimplemented_stub

### osa (2 verbs)

- `compile` - unimplemented_stub
- `getsource` - unimplemented_stub

### pict (4 verbs)

- `scheduleupdate` - unimplemented_stub
- `expressions` - unimplemented_stub
- `getpicture` - unimplemented_stub
- `setpicture` - unimplemented_stub

### point (2 verbs)

- `get` - unimplemented_stub
- `set` - unimplemented_stub

### python (1 verbs)

- `doscript` - unimplemented_stub

### re (10 verbs)

- `compile` - unimplemented_stub
- `match` - unimplemented_stub
- `replace` - unimplemented_stub
- `extract` - unimplemented_stub
- `split` - unimplemented_stub
- `join` - unimplemented_stub
- `visit` - unimplemented_stub
- `grep` - unimplemented_stub
- `getpatterninfo` - unimplemented_stub
- `expand` - unimplemented_stub

### rectangle (2 verbs)

- `get` - unimplemented_stub
- `set` - unimplemented_stub

### rez (15 verbs)

- `getresource` - unimplemented_stub
- `putresource` - unimplemented_stub
- `getnamedresource` - unimplemented_stub
- `putnamedresource` - unimplemented_stub
- `countrestypes` - unimplemented_stub
- `getnthrestype` - unimplemented_stub
- `countresources` - unimplemented_stub
- `getnthresource` - unimplemented_stub
- `getnthresinfo` - unimplemented_stub
- `resourceexists` - unimplemented_stub
- `namedresourceexists` - unimplemented_stub
- `deleteresource` - unimplemented_stub
- `deletenamedresource` - unimplemented_stub
- `getresourceattributes` - unimplemented_stub
- `setresourceattributes` - unimplemented_stub

### rgb (2 verbs)

- `get` - unimplemented_stub
- `set` - unimplemented_stub

### script (13 verbs)

- `compile` - unimplemented_stub
- `uncompile` - unimplemented_stub
- `getcode` - unimplemented_stub
- `getlanguage` - unimplemented_stub
- `setlanguage` - unimplemented_stub
- `makecomment` - unimplemented_stub
- `uncomment` - unimplemented_stub
- `iscomment` - unimplemented_stub
- `getbreakpoint` - unimplemented_stub
- `setbreakpoint` - unimplemented_stub
- `clearbreakpoint` - unimplemented_stub
- `startprofile` - unimplemented_stub
- `stopprofile` - unimplemented_stub

### search (6 verbs)

- `reset` - unimplemented_stub
- `findnext` - unimplemented_stub
- `replace` - unimplemented_stub
- `replaceall` - unimplemented_stub
- `findtextdialog` - unimplemented_stub
- `replacetextdialog` - unimplemented_stub

### searchengine (5 verbs)

- `stripmarkup` - unimplemented_stub
- `deindexpage` - unimplemented_stub
- `indexpage` - unimplemented_stub
- `cleanindex` - unimplemented_stub
- `mergeresults` - unimplemented_stub

### semaphore (2 verbs)

- `lock` - unimplemented_stub
- `unlock` - unimplemented_stub

### speaker (3 verbs)

- `beep` - unimplemented_stub
- `sound` - unimplemented_stub
- `playnamedsound` - unimplemented_stub

### sqlite (17 verbs)

- `open` - unimplemented_stub
- `compileQuery` - unimplemented_stub
- `clearQuery` - unimplemented_stub
- `resetQuery` - unimplemented_stub
- `stepQuery` - unimplemented_stub
- `getColumnCount` - unimplemented_stub
- `getColumnType` - unimplemented_stub
- `getColumnInt` - unimplemented_stub
- `getColumnDouble` - unimplemented_stub
- `getColumnText` - unimplemented_stub
- `getColumnName` - unimplemented_stub
- `getColumn` - unimplemented_stub
- `getRow` - unimplemented_stub
- `getErrorMessage` - unimplemented_stub
- `close` - unimplemented_stub
- `setColumnBlob` - unimplemented_stub
- `getLastInsertRowId` - unimplemented_stub

### statusbar (5 verbs)

- `msg` - unimplemented_stub
- `setsections` - unimplemented_stub
- `getsections` - unimplemented_stub
- `getsectionone` - unimplemented_stub
- `getmessage` - unimplemented_stub

### string (60 verbs)

- `delete` - missing
- `insert` - missing
- `popleading` - missing
- `poptrailing` - missing
- `trimwhitespace` - missing
- `popsuffix` - missing
- `hassuffix` - missing
- `mid` - missing
- `nthchar` - missing
- `nthfield` - missing
- `countfields` - missing
- `setwordchar` - missing
- `getwordchar` - missing
- `firstword` - missing
- `lastword` - missing
- `nthword` - missing
- `countwords` - missing
- `commentdelete` - missing
- `firstsentence` - missing
- `patternmatch` - missing
- `hex` - missing
- `timestring` - missing
- `datestring` - missing
- `upper` - missing
- `lower` - missing
- `filledstring` - missing
- `addcommas` - missing
- `replace` - missing
- `replaceall` - missing
- `length` - missing
- `isalpha` - missing
- `isnumeric` - missing
- `ispunctuation` - missing
- `processhtmlmacros` - missing
- `urldecode` - missing
- `urlencode` - missing
- `parsehttpargs` - missing
- `iso8859encode` - missing
- `getgifheightwidth` - missing
- `getjpegheightwidth` - missing
- `wrap` - missing
- `davenetmassager` - missing
- `parseaddress` - missing
- `dropnonalphas` - missing
- `padwithzeros` - missing
- `ellipsize` - missing
- `innercasename` - missing
- `urlsplit` - missing
- `hashMD5` - missing
- `latintomac` - missing
- `mactolatin` - missing
- `utf16toansi` - missing
- `utf8toansi` - missing
- `ansitoutf8` - missing
- `ansitoutf16` - missing
- `multiplereplaceall` - missing
- `macromantoutf8` - missing
- `utf8tomacroman` - missing
- `convertcharset` - missing
- `ischarsetavailable` - missing

### sys (16 verbs)

- `osversion` - unimplemented_stub
- `systemtask` - unimplemented_stub
- `browsenetwork` - unimplemented_stub
- `appisrunning` - unimplemented_stub
- `frontmostapp` - unimplemented_stub
- `bringapptofront` - unimplemented_stub
- `countapps` - unimplemented_stub
- `getnthapp` - unimplemented_stub
- `getapppath` - unimplemented_stub
- `memavail` - unimplemented_stub
- `machine` - unimplemented_stub
- `os` - unimplemented_stub
- `getenvironmentvariable` - unimplemented_stub
- `setenvironmentvariable` - unimplemented_stub
- `unixshellcommand` - unimplemented_stub
- `winshellcommand` - unimplemented_stub

### table (18 verbs)

- `move` - missing
- `copy` - missing
- `rename` - missing
- `moveandrename` - missing
- `assign` - missing
- `validate` - missing
- `sortby` - missing
- `getcursor` - missing
- `getselection` - missing
- `go` - missing
- `goto` - missing
- `gotoname` - missing
- `jettison` - missing
- `packtable` - missing
- `emptytable` - missing
- `getdisplaysettings` - missing
- `setdisplaysettings` - missing
- `getsortorder` - missing

### target (3 verbs)

- `get` - unimplemented_stub
- `set` - unimplemented_stub
- `clear` - unimplemented_stub

### tcp (23 verbs)

- `addressdecode` - unimplemented_stub
- `addressencode` - unimplemented_stub
- `addresstoname` - unimplemented_stub
- `nametoaddress` - unimplemented_stub
- `myaddress` - unimplemented_stub
- `abortstream` - unimplemented_stub
- `closestream` - unimplemented_stub
- `closelisten` - unimplemented_stub
- `openaddrstream` - unimplemented_stub
- `opennamestream` - unimplemented_stub
- `readstream` - unimplemented_stub
- `writestream` - unimplemented_stub
- `listenstream` - unimplemented_stub
- `statusstream` - unimplemented_stub
- `getpeeraddress` - unimplemented_stub
- `getpeerport` - unimplemented_stub
- `writestringtostream` - unimplemented_stub
- `writefiletostream` - unimplemented_stub
- `readstreamuntil` - unimplemented_stub
- `readstreambytes` - unimplemented_stub
- `readstreamuntilclosed` - unimplemented_stub
- `getstats` - unimplemented_stub
- `countconnections` - unimplemented_stub

### thread (17 verbs)

- `exists` - unimplemented_stub
- `evaluate` - unimplemented_stub
- `callscript` - unimplemented_stub
- `getcurrentid` - unimplemented_stub
- `getcount` - unimplemented_stub
- `getnthid` - unimplemented_stub
- `sleep` - unimplemented_stub
- `sleepfor` - unimplemented_stub
- `sleepticks` - unimplemented_stub
- `issleeping` - unimplemented_stub
- `wake` - unimplemented_stub
- `kill` - unimplemented_stub
- `gettimeslice` - unimplemented_stub
- `settimeslice` - unimplemented_stub
- `getdefaulttimeslice` - unimplemented_stub
- `setdefaulttimeslice` - unimplemented_stub
- `getstats` - unimplemented_stub

### webserver (7 verbs)

- `server` - unimplemented_stub
- `dispatch` - unimplemented_stub
- `parseheaders` - unimplemented_stub
- `parsecookies` - unimplemented_stub
- `buildresponse` - unimplemented_stub
- `builderrorpage` - unimplemented_stub
- `getserverstring` - unimplemented_stub

### window (31 verbs)

- `isopen` - missing
- `open` - missing
- `isfront` - missing
- `bringtofront` - missing
- `sendtoback` - missing
- `frontmost` - missing
- `next` - missing
- `isvisible` - missing
- `show` - missing
- `hide` - missing
- `close` - missing
- `update` - missing
- `ismenuscript` - missing
- `getposition` - missing
- `setposition` - missing
- `getsize` - missing
- `setsize` - missing
- `zoom` - missing
- `runselection` - missing
- `scroll` - missing
- `msg` - missing
- `dbstats` - missing
- `quickscript` - missing
- `ismodified` - missing
- `setmodified` - missing
- `gettitle` - missing
- `settitle` - missing
- `about` - missing
- `getfile` - missing
- `isreadonly` - missing
- `setquickscript` - missing

### xml (14 verbs)

- `addtable` - missing
- `addvalue` - missing
- `compile` - missing
- `decompile` - missing
- `getaddress` - missing
- `getaddresslist` - missing
- `getattribute` - missing
- `getattributevalue` - missing
- `getvalue` - missing
- `valtostring` - missing
- `frontiervaluetotaggedtext` - missing
- `structtofrontiervalue` - missing
- `getpathaddress` - missing
- `converttodisplayname` - missing


## Next Steps

### Phase 2: Systematic Testing
1. Create test harness to run all verbs
2. Document which verbs work in headless mode
3. Categorize failures into actionable buckets
4. Build implementation roadmap based on real data

### Phase 3: Targeted Implementation
1. Start with high-impact processors (string, date, dialog)
2. Implement platform-specific versions for macOS/Linux
3. Document GUI-incompatible verbs and provide error messages
4. Test incrementally with real UserTalk scripts

---

*This report was generated programmatically by analyzing kernelverbs.rc and*
*implementation files. Verb names are extracted directly from the RC file*
*rather than hardcoded mappings.*