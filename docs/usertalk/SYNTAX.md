# UserTalk Syntax Reference

Quick reference for UserTalk syntax rules and gotchas.

---

## String Quotes

**CRITICAL**: Double quotes for strings, single quotes for character constants!

```usertalk
sizeOf("hello")  // ✅ CORRECT - double quotes for strings
sizeOf('hello')  // ❌ WRONG - syntax error (single quotes = char constant)
```

**Why this matters:** This is the **opposite** of JavaScript/Python where `'x'` and `"x"` are equivalent. UserTalk treats single quotes as character constants (like C).

---

## Comments

### Block Comments (Safe Everywhere)

```usertalk
/* This is a comment */
local(x = 1)

/*
 * Multi-line comment
 * Works everywhere
 */
```

### Inline Comments (Top-Level Only)

**CRITICAL**: Inline `//` comments only work at top level, NOT inside code blocks!

```usertalk
// ✅ CORRECT - top level comment
local(x = 1)

on handler() {
	// ❌ WRONG - inline comment inside block causes parse errors!
	return true
}

on handler() {
	/* ✅ CORRECT - block comment works inside blocks */
	return true
}
```

**Reason**: UserTalk parser limitation - inline comments only work at file level, not inside code blocks.

---

## Indentation

### Blank Lines Inside Blocks

**CRITICAL**: Blank lines inside indented blocks must match the surrounding indentation level!

```usertalk
// ❌ WRONG - blank line with no indentation
on handler() {
	local(x = 1)

	return x  // Parse error - blank line above has wrong indentation
}

// ✅ CORRECT - blank line matches indentation
on handler() {
	local(x = 1)

	return x  // Blank line above has same indentation as surrounding code
}

// ✅ SIMPLEST - avoid blank lines inside blocks entirely
on handler() {
	local(x = 1)
	return x
}
```

**Reason**: UserTalk file parser requires indentation level to match previous line, even for blank lines.

**Best practice**: Use compact formatting without blank lines inside handlers/blocks.

---

## Historical Context

**Why these gotchas exist:**

UserTalk was designed for outline editing in the original Frontier environment:
- Indentation was automatic (handled by outline editor)
- Braces `{`, `}`, and semicolons `;` were rarely typed manually
- Text-based editing was not the primary workflow

Modern text-based development requires awareness of these parser constraints.

---

## See Also

- **typeof() behavior:** `docs/usertalk/TYPEOF.md`
- **File operations:** `docs/usertalk/FILE_AND_DB.md`
- **Test patterns:** `docs/TESTING_GUIDE.md` § UserTalk Test Patterns
