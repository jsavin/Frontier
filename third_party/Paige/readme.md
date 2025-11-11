# Introduction

**Paige**, maintained by Team HERMES, is an open-source, cross-platform library that enables you to develop your own apps capable of displaying and manipulating long-form styled text. This is an almost impossibly general description, but only because the range of applicability is equally broad. The trivial use case for Paige is as the core of a word processor or an eBook viewer---obviously, in this role, it functions perfectly.  Paige can respond to commands to use various fonts, place text within a multimedia document, and reserve other places within the document for other items, such as image data or video.

In the hands of a team with vision and imagination, however, Paige is much more than that. For instance, think of a Markdown editor: in most cases, it presents the user with a two-pane view, with raw Markdown in the left pane and styled text in the right. In many cases, both panes are editable; sometimes, however, only the raw syntax is. No matter the paradigm the developer chooses in this case, and no matter his creative decisions, Paige is the ideal choice to power this application at both ends.

Likewise, Paige is a great option for cross-platform apps. The Microsoft Foundation Classes include the `CRichEditCtrl` class, making this the natural choice for developers of Windows applications, and likewise the Haiku operating system implements the BTextView class, but both of these are platform-specific. Realistically, for a cross-platform application, there are two alternatives: the Scribe framework available in Qt (and its `QTextDocument` class), and GTK with its combination of `GtkTextBuffer` and `GtkTextView`.
 
There are two important differences between Qt/GTK and Paige. First of all, Qt and GTK originally arose from the UNIX ecosystem, and thus use UNIX conventions; meanwhile, Paige was cross-platform for Windows and Macintosh from the get-go, and thus may be more comfortable for the Windows programmer to get used to. Second, Qt and GTK are *generalist* toolkits, and their rich-edit controls are similarly limited; Paige is designed specifically for working with styled text---and nothing else.

Finally, HERMES Paige is **meticulously** documented. The programmer's manual for the latest release, Version 3.0, runs to over 800 pages; it is slated to be published in May 2024 on Lulu and Amazon, but is also included as a soft copy here.

## Users

During its previous life as a product of DataPak Software in Vancouver, Washington, Paige was widely used, not only by hobbyists and one-man shops but also by blue-chip corporations in the software-development space. The applications listed here represent just some of the hundreds of Paige users at one point in history or another.

* Director 5.0 by Macromedia
* OmniPage by Caere Corp.
* Crystal Reports Professional 5.0 by Seagate Software
* ClickWorks by Scitex America Corp.
* a WYSIWYG web editor by Animatrix, Inc. in co-operation with Chilehead Software 
* Panorama 6 by ProVue Development
* Eudora 7 for Windows by Qualcomm Inc.


# Supplied Materials

## 3.0b - 26 Apr 2024

* 32 bit, Unicode binary distribution as W

## 3.0a - 22 Apr 2024

* initial build - source code available on GitHub
* 32 bit binary distribution as Windows DLL on Releases page 
    * not the Unicode version - this is still being planned and debugged 
* 64 bit binary distribution as Windows DLL is also in debug stage
