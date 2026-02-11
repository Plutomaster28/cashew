# Contributing to Cashew

Thanks for your interest in contributing to Cashew.

This document outlines how to contribute effectively.

## Before You Start

Read the [README](README.md) and [PROTOCOL_SPEC.md](PROTOCOL_SPEC.md) to understand what Cashew is and how it works.

If you're not sure whether something fits the project, open an issue first.

## Ways to Contribute

### Reporting Bugs

- Check existing issues first.
- Provide minimal reproduction steps.
- Include system info (OS, compiler version, hardware).
- Paste relevant logs or error messages.

### Suggesting Features

- Explain the use case clearly.
- Stay within the project's scope (see Non-Goals below).
- Be prepared for "no" if it doesn't align with the design.

### Submitting Pull Requests

1. **Fork and clone** the repository.
2. **Create a branch** for your changes (`git checkout -b feature/my-feature`).
3. **Write code** following the project style (see below).
4. **Test thoroughly** - run existing tests and add new ones if needed.
5. **Commit with clear messages** - explain what and why, not just what.
6. **Push and open a PR** - reference related issues if applicable.

### Code Review Expectations

- Maintainers will review PRs when time permits.
- Expect feedback - be prepared to iterate.
- Large PRs may take longer to review.
- Breaking changes require strong justification.

## Development Setup

### Prerequisites

- CMake 3.20+
- C++20 compiler (GCC 11+, Clang 14+, MSVC 2022)
- Ninja (recommended)
- libsodium, spdlog, nlohmann_json

### Building

```bash
mkdir build && cd build
cmake -G Ninja ..
ninja
```

See [BUILD.md](BUILD.md) for detailed instructions.

### Running Tests

```bash
cd build
./tests/test_crypto
./tests/test_identity
```

More tests will be added as development progresses.

## Code Style

Cashew uses standard C++20 idioms with a focus on clarity and performance.

### General Guidelines

- **Naming**: `snake_case` for variables/functions, `PascalCase` for types.
- **Headers**: Use `#pragma once`.
- **Includes**: Group by category (system, third-party, project).
- **Formatting**: 4 spaces, no tabs. Follow existing style.
- **Comments**: Explain why, not what (unless non-obvious).
- **Error handling**: Use `std::optional`, exceptions only for unrecoverable errors.

### Example

```cpp
// Good
bool validate_content(const Hash256& hash, const std::vector<uint8_t>& data) {
    auto computed = crypto::Blake3::hash(data);
    return computed == hash;
}

// Bad
bool ValidateContent(const Hash256 &hash, const std::vector<uint8_t> &data)
{
  auto computed=crypto::Blake3::hash(data);
  return computed==hash;
}
```

### Documentation

- Public APIs should have docstrings.
- Tricky algorithms should have explanatory comments.
- Update relevant markdown files if changing behavior.

## Testing

- Add unit tests for new functionality.
- Ensure existing tests pass before submitting.
- Integration tests are appreciated but not required for small changes.

## Commit Messages

Write clear, concise commit messages:

```
Add content integrity verification to gateway

- Verify BLAKE3 hash before serving content
- Add defense-in-depth check in ContentRenderer
- Log integrity failures with detailed error messages

Closes #42
```

Avoid vague messages like "fix bug" or "update code".

## Pull Request Process

1. Ensure your code builds without warnings.
2. Update documentation if you've changed public APIs.
3. Add tests if you've added functionality.
4. Keep PRs focused - one feature or fix per PR.
5. Be responsive to feedback.

Maintainers will merge when satisfied. No guarantees on timeline.

## Non-Goals

Cashew is a low-level systems framework.

The following are explicitly out of scope:

- Java or JVM-based integrations
- JNI bindings
- Managed runtime wrappers
- High-level abstractions that obscure protocol behavior

Pull requests or issues proposing these will be closed.

Forks are free to experiment, but official Cashew development remains C/C++ only (Extremely crucial or modules that **do benefit** from it may be rewritten in Rust or ASM; it must be a genuine benefit too not just "cuz its better durrr").

## Communication

- **Issues**: Bug reports, feature requests, technical discussions.
- **Pull Requests**: Code contributions with clear descriptions.
- **Discussions**: Design questions, architecture thoughts.

Keep it technical. Keep it civil.

## License

By contributing, you agree that your contributions will be licensed under the same license as the project (see [LICENSE](LICENSE)).

## Questions?

Open an issue with the `question` label.

---

**Remember**: Cashew is infrastructure. Contributions should prioritize correctness, performance, and maintainability.
