function welcome() {
  return "Hello world!\n";
}

function welcomeWithCallback(callback) {
    callback("hello, world!");
}

function add(a, b) {
  return a + b;
}

add(2, 3);