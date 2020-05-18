Contributing to Entangle
========================

The Entangle projects accepts code contributions via merge requests on the
GitLab project:

https://gitlab.com/entangle/entangle/-/merge_requests

It is required that automated CI pipelines succeed before a merge request
will be accepted. The global pipeline status for the ``master`` branch is
visible at:

https://gitlab.com/entangle/entangle/pipelines

CI pipeline results for merge requests will be visible via the contributors'
own private repository fork:

https://gitlab.com/yourusername/entangle/pipelines

Note that if you disable CI in your forked project, then we will be unable to
accept any merge requests.

When opening merge requests it can be helpful to enable the checkbox for the
option:

```
  Allow commits from members who can merge to the target branch
```

This allows the maintainers to quickly make simple bug fixes to a merge request
without needing the original contributor's involvement. This can speed up the
process of getting changes merged.

Translations
------------

The work for translating Entangle has been outsourced to the Fedora Project
translation team. As such changes made to the po/*.po are only permitted to be
made by the Weblate translation software. If you are not a member of the Fedora
project translation team, and spot a bug in the translations that you believe
needs fixing, then please file a file against Entangle in GitLab. The fix will
be taken from the bug and fed into Weblate for approval.

Code formatting
---------------

All the C source files are expected to be formatted using the clang-format
tool. There is a configuration file for clang-format in the root of the project
source tree. Authors are advised to configure their editor to automate the
running of clang-format when saving files:

https://clang.llvm.org/docs/ClangFormat.html

There are various coding rules beyond the basic formatting that must be complied
with. These are known as the "syntax check" rules, and are validated by the
test suite registered with meson. Again these are validatde

GitLab CI jobs will validate that patches comply with both the clang-format and
syntax check rules.

Camera support
--------------

Entangle uses the libgphoto2 library for controlling cameras. While it aims
to provide a consistent API covering different models of camera, there are none
the less many differences in behaviour between cameras, especially across the
vendors. If touching any code that directly interacts with the cameras, it is
important to consider behaviour across different cameras. An operation that
works on Canon DSLRs may not work on Nikon DSLRs, or vica-versa. Contributors
are of course not expected to test with every possible camera themselves. Just
mention in any merge requests what cameras have been tested with, and which
version of libgphoto2 was used.
