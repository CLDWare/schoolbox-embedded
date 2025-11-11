updatedb:
	rm -f compile_commands.json && pio run -t compiledb

build:
	pio run

run:
	pio run --target upload

clean:
	pio run --target clean

monitor:
	pio device monitor
