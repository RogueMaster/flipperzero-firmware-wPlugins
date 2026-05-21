local flipper_sdk_path=vim.fn.expand("~/.ufbt/current/sdk_headers/f7_sdk/")
vim.opt.path:append({
  flipper_sdk_path.."furi",
  flipper_sdk_path.."applications/services",
})

local overseer = require('overseer')

overseer.register_template({
  name = 'Build',
  builder = function(params)
    return {
      cmd = { "ufbt" },
      components = {
        "on_exit_set_status",
        "on_output_quickfix",
        {
          "open_output",
          direction = "horizontal",
          focus = true,
          on_start = "always",
        },
        "unique",
      },
    }
  end,
  tags = { 'BUILD' },
})

overseer.register_template({
  name = 'Launch',
  builder = function(params)
    return {
      cmd = { "ufbt", "launch" },
    }
  end,
  tags = { 'RUN' },
})
