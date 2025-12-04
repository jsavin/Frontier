- Frontier has a concept of "guest databases" which are any databases that are opened that aren't 
the system root. All top-level items in guest databases are in global scope in the UserTalk domain. This is managed by the
kernel leveraging the in-memory "table" at system.compiler.files.
- Frontier has the concept of the current "target" which is generally a window. That might be a database or it might be an editor window for a non-scalar like a script, outline, or WPText object (which we're now persisting as RTF in UTF-8).
- Legacy Frontier source code is available at /Users/jake/dev/tedchoward/Frontier