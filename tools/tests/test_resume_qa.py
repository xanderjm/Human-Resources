"""Diagnostic transport cannot implicitly resume, replace or bank a run."""
from pathlib import Path
import json
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


class ResumeQaContracts(unittest.TestCase):
    @unittest.skipUnless(shutil.which('c++'), 'standalone C++ compiler unavailable')
    def test_explicit_actions_and_restore_diagnostics(self):
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / 'contracts.cpp'
            source.write_text(r'''
#include "resume_qa.h"
#include <cassert>
#include <iostream>
int main() {
  using namespace human_resources;
  using product::Screen; using product::Command;
  client::ResumeQa qa;
  assert(!qa.configure("inspect", "../normal"));
  assert(!qa.configure("invalid", "slot"));
  assert(!qa.active());
  assert(qa.configure("prepare-choice", "device_qa-1"));
  assert(qa.next_command(Screen::pause, false, true) == Command::none);
  assert(qa.next_command(Screen::title, true, false) == Command::none);
  assert(qa.next_command(Screen::title, false, false) == Command::start);
  assert(qa.next_command(Screen::title, false, false) == Command::none);
  assert(qa.next_command(Screen::upgrade, false, false) == Command::none);
  assert(qa.take_hold_marker(Screen::upgrade));
  assert(!qa.take_hold_marker(Screen::upgrade));
  assert(qa.configure("inspect", "slot"));
  for (auto screen : {Screen::title, Screen::pause, Screen::upgrade, Screen::results})
    assert(qa.next_command(screen, false, true) == Command::none);
  assert(qa.movement_x(10) == 0);
  assert(qa.configure("resume", "slot"));
  assert(qa.next_command(Screen::pause, false, false) == Command::none);
  assert(qa.next_command(Screen::pause, false, true) == Command::resume);
  assert(qa.next_command(Screen::upgrade, false, true) == Command::none);
  assert(qa.configure("bank", "slot"));
  assert(qa.next_command(Screen::pause, true, true) == Command::none);
  assert(qa.next_command(Screen::pause, false, true) == Command::finish);
  assert(qa.next_command(Screen::pause, false, true) == Command::none);
  gameplay::Snapshot state; state.phase = gameplay::Phase::choosing_upgrade;
  state.choices[0].title = "Quoted \"choice\"";
  state.choices[0].description = "line\nnext";
  for (const auto& line : client::resume_log_lines(state, 17, 2, 123)) std::cout << line << '\n';
}
''')
            executable = Path(directory) / 'contracts'
            command = ['c++', '-std=c++20', '-O2', '-Wall', '-Wextra', '-Wconversion', '-Wsign-conversion', '-Werror']
            for include in sorted((ROOT / 'native').glob('*/include')):
                command.extend(['-I', str(include)])
            command.extend(['-I', str(ROOT / 'native/human_resources_client/src'), str(source),
                            str(ROOT / 'native/human_resources_client/src/resume_qa.cpp'), '-o', str(executable)])
            subprocess.run(command, check=True, capture_output=True)
            output = subprocess.check_output([str(executable)], text=True)
            detail = json.loads(output.removeprefix('product-run-detail '))
            self.assertEqual((detail['run'], detail['location'], detail['phase']), (17, 2, 3))
            self.assertEqual(len(detail['choices']), 3)
            self.assertEqual(detail['choices'][0]['description'], 'line\nnext')
