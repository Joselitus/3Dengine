#include "EspeakSynthesizer.h"

#include <csignal>
#include <iostream>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

using namespace std;

shared_ptr<AudioClip> EspeakSynthesizer::synthesize(const string &text,
                                                    const VoiceSettings &voice) {
  int input[2], output[2]; // pipes: [0] read end, [1] write end
  if (pipe(input) != 0)
    return nullptr;
  if (pipe(output) != 0) {
    close(input[0]);
    close(input[1]);
    return nullptr;
  }

  string speed = to_string(voice.wordsPerMinute);
  string pitch = to_string(voice.pitch);
  pid_t child = fork();
  if (child < 0) {
    for (int fd : {input[0], input[1], output[0], output[1]})
      close(fd);
    return nullptr;
  }
  if (child == 0) {
    // The child: stdin from `input`, stdout to `output`, then espeak-ng
    dup2(input[0], STDIN_FILENO);
    dup2(output[1], STDOUT_FILENO);
    for (int fd : {input[0], input[1], output[0], output[1]})
      close(fd);
    execlp(program.c_str(), program.c_str(), "-v", voice.language.c_str(),
           "-s", speed.c_str(), "-p", pitch.c_str(), "--stdin", "--stdout",
           (char *)nullptr);
    _exit(127); // exec failed
  }

  close(input[0]);
  close(output[1]);
  // A closed pipe must not kill the game with SIGPIPE
  signal(SIGPIPE, SIG_IGN);
  // espeak-ng reads all its input before it starts writing, so the text can
  // be sent whole first (it is short) without risk of both sides waiting
  string message = text + "\n";
  for (size_t sent = 0; sent < message.size();) {
    ssize_t n = write(input[1], message.data() + sent, message.size() - sent);
    if (n <= 0)
      break;
    sent += n;
  }
  close(input[1]);

  string wav;
  vector<char> chunk(1 << 16);
  ssize_t n;
  while ((n = read(output[0], chunk.data(), chunk.size())) > 0)
    wav.append(chunk.data(), n);
  close(output[0]);

  int status = 0;
  waitpid(child, &status, 0);
  if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
    cerr << "EspeakSynthesizer: '" << program << "' failed"
         << (WIFEXITED(status) && WEXITSTATUS(status) == 127
                 ? " (is it installed?)"
                 : "")
         << endl;
    return nullptr;
  }
  shared_ptr<AudioClip> clip = make_shared<AudioClip>();
  if (!clip->loadWav(wav)) {
    cerr << "EspeakSynthesizer: unexpected output from " << program << endl;
    return nullptr;
  }
  return clip;
}
