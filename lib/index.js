const childProcess = require('child_process');
const util = require('util');

function forkNative (exec, args, options) {
  // Get options and args arguments.
  var options, args;
  if (Array.isArray(arguments[1])) {
    args = arguments[1];
    options = util._extend({}, arguments[2]);
  } else if (arguments[1] && typeof arguments[1] !== 'object') {
    throw new TypeError('Incorrect value of args option');
  } else {
    args = [];
    options = util._extend({}, arguments[1]);
  }

  if (!Array.isArray(options.stdio)) {
    // Use a separate fd=3 for the IPC channel. Inherit stdin, stdout,
    // and stderr from the parent if silent isn't set.
    options.stdio = options.silent ? ['pipe', 'pipe', 'pipe', 'ipc'] :
      [0, 1, 2, 'ipc'];
  } else if (options.stdio.indexOf('ipc') === -1) {
    throw new TypeError('Forked processes must have an IPC channel');
  }

  options.execPath = options.execPath || process.execPath;
  options.shell = false;
  
  return childProcess.spawn(exec, args, options);
}

module.exports = { forkNative, ...childProcess };
