const net = require('net')

process.on('message', (msg, handle) => {
  switch(msg.cmd) {
    case 'connect':
      let c = net.createConnection(msg.port)
      c.setEncoding('utf8')
      c.on('data', (data) => {
        process.send(JSON.parse(data))
      })
      break
  }
})

