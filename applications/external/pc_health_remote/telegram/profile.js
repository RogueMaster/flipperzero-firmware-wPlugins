// Public Telegram profile of @PCHealthRemoteBot, applied by admin /brand (bot-profile.js). Command menus come
// from COMMANDS in worker.js. English default (the bot answers in English or Russian); uk/ru for those clients.
export const PROFILE = {
  avatar: "pchealth",
  default: "en",
  langs: {
    en: {
      short: "Alerts from your PC to Telegram: CPU/GPU temperature and load over your limits. Free app, MIT.",
      description:
        "🖥 PC Health Remote watches your Windows PC and messages you when something goes wrong.\n\n" +
        "🔥 CPU and GPU temperature and load over your limits, with the value and the limit\n" +
        "🔗 Link a PC in 1 minute: /link, then one command on the PC\n" +
        "🆓 Free: 1 PC, 10 alerts a day. Pro 99 ⭐ / 30 days: up to 5 PCs, unlimited alerts\n\n" +
        "The PC app is free and open source (MIT). No message text or keys in logs.",
    },
    uk: {
      short: "Сповіщення з вашого ПК у Telegram: температура й навантаження CPU/GPU понад ліміт. Безкоштовний застосунок, MIT.",
      description:
        "🖥 PC Health Remote стежить за вашим ПК на Windows і пише, коли щось не так.\n\n" +
        "🔥 Температура й навантаження CPU та GPU понад ваші ліміти, зі значенням і лімітом\n" +
        "🔗 Прив’язка ПК за 1 хвилину: /link, потім одна команда на ПК\n" +
        "🆓 Безкоштовно: 1 ПК, 10 сповіщень на день. Pro 99 ⭐ / 30 днів: до 5 ПК, без ліміту\n\n" +
        "Застосунок для ПК безкоштовний і відкритий (MIT). Бот відповідає англійською.",
    },
    ru: {
      short: "Уведомления с вашего ПК в Telegram: температура и нагрузка CPU/GPU выше лимита. Бесплатное приложение, MIT.",
      description:
        "🖥 PC Health Remote следит за вашим ПК на Windows и пишет, когда что-то не так.\n\n" +
        "🔥 Температура и нагрузка CPU и GPU выше ваших лимитов, со значением и лимитом\n" +
        "🔗 Привязка ПК за 1 минуту: /link, затем одна команда на ПК\n" +
        "🆓 Бесплатно: 1 ПК, 10 уведомлений в день. Pro 99 ⭐ / 30 дней: до 5 ПК, без лимита\n\n" +
        "Приложение для ПК бесплатное и открытое (MIT). Логи без текста сообщений и ключей.",
    },
  },
};
