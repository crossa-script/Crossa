require "socket"

handler = ARGV.fetch(0)
server = TCPServer.new("127.0.0.1", 18_765)

loop do
    client = server.accept
    Thread.new(client) do |connection|
        request = +""
        until request.include?("\r\n\r\n")
            request << connection.readpartial(4_096)
        end

        headers, body = request.split("\r\n\r\n", 2)
        content_length = headers[/\r\nContent-Length:\s*(\d+)/i, 1].to_i
        while body.bytesize < content_length
            body << connection.readpartial(4_096)
        end

        IO.popen(["/bin/bash", handler], "r+") do |process|
            process.write(headers)
            process.write("\r\n\r\n")
            process.write(body)
            process.close_write
            connection.write(process.read)
        end
    rescue EOFError
    ensure
        connection.close
    end
end
