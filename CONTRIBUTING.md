# Contributing to RTLPlayground

## AI-assisted contributions

AI tools are welcome, but a human must stay in the loop. You are responsible for your contribution, whoever or whatever generated it, and you must be able to explain it during review.

Before opening a pull request, and before every push to it, you must:

- keep the agent to the agreed scope and reject unrelated changes;
- review every changed file and understand the resulting behavior;
- have a separate agent session or a person adversarially review the full pull request diff against its base branch, then fix each finding or explain in the pull request description why it does not apply; and
- build the affected targets and test the changed behavior yourself, then record the result for the pull request description: the kind of check (host test, simulator or hardware), the board, the build, and anything that failed or remains untested.

Only after completing these steps do you open the pull request or push to it. An agent may draft pull requests and comments, but nothing is published without your approval.

### Pull request description

Every pull request body must include at least one sentence written by you, in your own words, explaining what changed and why. A generated summary, pasted agent transcript or checklist alone does not satisfy this requirement.

### Edit what the tool writes

Review and edit AI-generated text before you post it in a pull request, issue or comment. Trim it to what a reviewer needs: what changed, why and how it was checked, in plain and precise sentences. Label guesses as guesses.

AI tools tend to pad text with noise that hides the main point. Remove transcripts, internal plans, repeated summaries, generic praise, sales language, claims of excellence and unsolicited offers to do more.

Respond to review feedback yourself, and only apply suggestions you have checked.

## Contribution licensing

A contribution intentionally submitted for inclusion in RTLPlayground is licensed under the MIT License.

This policy does not relicense third-party or vendored code. You must have the right to submit your contribution and must preserve applicable copyright, license, attribution and notice material. Submitting a contribution does not require signing a Contributor License Agreement (CLA) or certifying a Developer Certificate of Origin (DCO).

## References

This policy is based on [oh-my-pi](https://github.com/can1357/oh-my-pi/blob/main/CONTRIBUTING.md), the [LLVM AI Tool Use Policy](https://llvm.org/docs/AIToolPolicy.html) and the [Ghostty AI Usage Policy](https://github.com/ghostty-org/ghostty/blob/main/AI_POLICY.md).
