/*
 * drivers/input/mouse/vd5376_mouse_right.c
 *
 * Copyright (c) 2010 Crztech Co., Ltd.
 *	Pyeongjeong Lee <leepjung@crz-tech.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/vd5376.h>
#include "vd5376_mouse.h"

#include <plat/gpio-cfg.h>
#include <linux/gpio.h>
#include <linux/delay.h>

#define DEVICE_NAME	"vd5376"

static struct workqueue_struct *vd5376_wq;

static void vd5376_work_func(struct work_struct *work)
{
	int error;
	s8 data[2], old_data[2] = {0,0};
	struct vd5376 *mouse = container_of(work, struct vd5376, work.work);
	struct vd5376_platform_data *pdata = mouse->pdata;

	while(!gpio_get_value(pdata->gpio))
	{
		error = i2c_smbus_read_i2c_block_data(mouse->client,
					      Device_X_Motion, 
					      2, data);
		if(error < 0) {
			printk(KERN_ERR "i2c read error(%d)!\n", error);
			goto out;
		}

		if(old_data[0] == data[0] && old_data[1] == data[1])
			continue;

		old_data[0] = data[0];
		old_data[1] = data[1];

		input_report_rel(mouse->input, REL_X, data[0]);
		input_report_rel(mouse->input, REL_Y, data[1]);
		input_sync(mouse->input);

		udelay(10);
	}

out:
#ifdef CONFIG_VD5376_INTERRUPT_MOTION
	enable_irq(pdata->irq);
#endif
#ifdef CONFIG_VD5376_POLLING
	queue_delayed_work(vd5376_wq, &mouse->work,
			   msecs_to_jiffies(pdata->delay));
#endif
}

#ifdef CONFIG_VD5376_INTERRUPT_MOTION
static irqreturn_t vd5376_irq_handler(int irq, void *handle)
{
	struct vd5376 *mouse = handle;
	struct vd5376_platform_data *pdata = mouse->pdata;

	disable_irq_nosync(pdata->irq);

	if(irq == pdata->irq) {
		queue_delayed_work(vd5376_wq, &mouse->work,
				   msecs_to_jiffies(pdata->delay));
	} else {
		input_report_key(mouse->input, BTN_LEFT, gpio_get_value(pdata->btn_gpio)^(pdata->btn_active_low));
		input_sync(mouse->input);

		enable_irq(pdata->irq);
	}

	return IRQ_HANDLED;
}
#endif

static void vd5376_mouse_input_params(struct vd5376 *mouse)
{
	struct input_dev *input = mouse->input;

        input->name = mouse->client->name;//"vd5376-left";
        input->phys = mouse->client->adapter->name;//"vd5376-left";
        input->id.bustype = BUS_I2C;
        input->dev.parent = &mouse->client->dev;
        input_set_drvdata(input, mouse);

	input_set_capability(input, EV_REL, REL_X);
        input_set_capability(input, EV_REL, REL_Y);
	input_set_capability(input, EV_KEY, BTN_LEFT);
	input_set_capability(input, EV_KEY, BTN_RIGHT);
}

static void vd5376_mouse_gpio_params(struct vd5376 *mouse)
{
	int err;
	struct vd5376_platform_data *pdata = mouse->pdata;

	err = gpio_request(pdata->pd_gpio, pdata->pd_desc);
	if (err) {
		printk(KERN_INFO "gpio request error : %d\n", err);
	} else {
		s3c_gpio_setpull(pdata->pd_gpio, S3C_GPIO_PULL_NONE);
		gpio_direction_output(pdata->pd_gpio, 1);
	}
	gpio_free(pdata->pd_gpio);

#ifdef CONFIG_VD5376_INTERRUPT_MOTION
        err = gpio_request(pdata->btn_gpio, pdata->btn_desc);
	if (err) {
		printk(KERN_INFO "gpio request error : %d\n", err);
	} else {
		s3c_gpio_cfgpin(pdata->btn_gpio, pdata->btn_config);
		s3c_gpio_setpull(pdata->btn_gpio, S3C_GPIO_PULL_NONE);
	}
	gpio_free(pdata->btn_gpio);

        err = gpio_request(pdata->gpio, pdata->desc);
	if (err) {
		printk(KERN_INFO "gpio request error : %d\n", err);
	} else {
		s3c_gpio_cfgpin(pdata->gpio, pdata->config);
		s3c_gpio_setpull(pdata->gpio, S3C_GPIO_PULL_NONE);
	}
	gpio_free(pdata->gpio);
#endif
}

static int vd5376_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	int err, i;
	struct vd5376 *mouse;
	struct input_dev *input_dev;
	struct vd5376_platform_data *pdata = client->dev.platform_data;

	if (!i2c_check_functionality(client->adapter,
				     I2C_FUNC_SMBUS_READ_WORD_DATA)) {
		printk(KERN_ERR "%s: need I2C", __func__);
		return -ENODEV;
	}

	mouse = kzalloc(sizeof(struct vd5376), GFP_KERNEL);
	if (!mouse) {
		err = -ENOMEM;
		goto err_free;
	}

	mouse->client = client;
	mouse->pdata = pdata;

	input_dev = input_allocate_device();
	if (!input_dev) {
		err = -ENOMEM;
		printk(KERN_ERR "%s: Failed to allocate input device\n", 
			__func__);
		goto err_free_mem;
	}

	mouse->input = input_dev;

	vd5376_mouse_input_params(mouse);
	vd5376_mouse_gpio_params(mouse);
	
	udelay(100);

	for ( i = 0; i < VD5376_INIT_REGS; i++) {
		err = i2c_smbus_write_byte_data(mouse->client,
						vd5376_init_reg[i][0],
						vd5376_init_reg[i][1]);
		if (err < 0) break;
		if(i == 0) udelay(100);
	}

	if (err < 0) {
		dev_err(&client->dev, "%s : vd5376 initialization failed\n",
			__func__);
		goto err_free_device;
	}

	err = input_register_device(input_dev);
	if (err) {
		printk(KERN_ERR "%s: Unable to register %s input device\n",
			__func__, input_dev->name);
		goto err_free_device;
	}

	INIT_DELAYED_WORK(&mouse->work, vd5376_work_func);

#ifdef CONFIG_VD5376_INTERRUPT_MOTION
	err = request_irq(pdata->btn_irq, vd5376_irq_handler,
			  IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING,
			  client->name, mouse);
	if (err < 0) {
		dev_err(&client->dev, "motion irq %d busy?\n",
			pdata->btn_irq);
		goto err_free_register;
	}

	err = request_irq(pdata->irq, vd5376_irq_handler,
			  IRQF_TRIGGER_FALLING,
			  client->name, mouse);
	if (err < 0) {
		dev_err(&client->dev, "motion irq %d busy?\n",
			pdata->irq);
		goto err_free_register;
	}

	printk(KERN_INFO "%s: Start Finger Mouse %s in interrupt mode\n", 
		 __func__, input_dev->name);
#endif

#ifdef CONFIG_VD5376_POLLING
	queue_delayed_work(vd5376_wq, &mouse->work,
			   msecs_to_jiffies(mouse->delay));

	printk(KERN_INFO "%s: Start Finger Mouse %s in polling mode\n", 
		 __func__, input_dev->name);
#endif

	i2c_set_clientdata(client, mouse);

	return 0;

#ifdef CONFIG_VD5376_INTERRUPT_MOTION
 err_free_register:
	input_unregister_device(input_dev);
#endif
 err_free_device:
	input_free_device(input_dev);
 err_free_mem:
	kfree(mouse);
 err_free:
	return err;
}

static int vd5376_remove(struct i2c_client *client)
{
	struct vd5376 *mouse = i2c_get_clientdata(client);
	struct vd5376_platform_data *pdata = mouse->pdata;

#ifdef CONFIG_VD5376_INTERRUPT_MOTION
	free_irq(pdata->irq, mouse);
#endif
	cancel_delayed_work_sync(&mouse->work);
	input_unregister_device(mouse->input);
	input_free_device(mouse->input);
	kfree(mouse);

	return 0;
}

static struct i2c_device_id vd5376_idtable[] = {
	{ DEVICE_NAME, 0 },
	{ }
};

MODULE_DEVICE_TABLE(i2c, vd5376_idtable);

static struct i2c_driver vd5376_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= DEVICE_NAME
	},
	.id_table	= vd5376_idtable,
	.probe		= vd5376_probe,
	.remove		= vd5376_remove,
};

static int __init vd5376_init(void)
{
	vd5376_wq = create_singlethread_workqueue("vd5376_wq");
	if (!vd5376_wq)
		return -ENOMEM;

	return i2c_add_driver(&vd5376_driver);
}

static void __exit vd5376_exit(void)
{
	i2c_del_driver(&vd5376_driver);

	if (vd5376_wq)
		destroy_workqueue(vd5376_wq);
}

module_init(vd5376_init);
module_exit(vd5376_exit);

MODULE_AUTHOR("Pyeongjeong Lee <leepjung@crz-tech.com>");
MODULE_DESCRIPTION("VD5376 Finger Mouse Driver");
MODULE_LICENSE("GPL");

