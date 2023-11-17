#include "stdinclude.hpp"

namespace
{
	void console_thread()
	{
		std::string line;

		while (true)
		{
			std::cin >> line;

			std::cout << "\n] " << line << "\n";

			if (line == "reload")
			{
				std::ifstream config_stream {"config.json"};
				std::vector<std::string> dicts {};

				rapidjson::IStreamWrapper wrapper {config_stream};
				rapidjson::Document document;

				document.ParseStream(wrapper);

				if (!document.HasParseError())
				{
					auto& dicts_arr = document["dicts"];
					auto len = dicts_arr.Size();

					for (size_t i = 0; i < len; ++i)
					{
						auto dict = dicts_arr[i].GetString();

						dicts.push_back(dict);
					}
				}

				config_stream.close();

			}
		}
	}
}

void start_console()
{
#ifdef _DEBUG
	std::thread(console_thread).detach();
#endif
}